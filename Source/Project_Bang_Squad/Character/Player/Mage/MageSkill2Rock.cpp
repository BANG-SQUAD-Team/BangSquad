
#include "Project_Bang_Squad/Character/Player/Mage/MageSkill2Rock.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"


AMageSkill2Rock::AMageSkill2Rock()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	SetReplicateMovement(false);
	
	
	// 1. 충돌체 설정
	OverlapComp = CreateDefaultSubobject<USphereComponent>("OverlapComp");
	RootComponent = OverlapComp;
	OverlapComp->SetSphereRadius(200.0f); // 감지 범위
	OverlapComp->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // 겹침 허용
	
	// 2. 바위 메쉬 설정
	RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
	RockMesh->SetupAttachment(RootComponent);
	RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메쉬 자체는 물리 충돌 없음
	
	// 처음에는 바닥 아래에 숨겨둠
	RockMesh->SetRelativeLocation(FVector(0.f, 0.f ,-200.f));
	
	RockMesh->SetVisibility(false);
	
	// 나이아가라 컴포넌트 생성 및 부착
	NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComp"));
	NiagaraComp->SetupAttachment(RootComponent); // 바위 중심에 부착
    
	// 태어날 때는 꺼둠 (솟아오를 때 켤 것임)
	NiagaraComp->bAutoActivate = false; 
    
	// 바위 메쉬보다 약간 아래 배치 (흙먼지 느낌)
	NiagaraComp->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
}

void AMageSkill2Rock::InitializeRock(float DamageAmount, APawn* InstigatorPawn)
{
	SkillDamage = DamageAmount;
	CasterPawn = InstigatorPawn;
}


void AMageSkill2Rock::BeginPlay()
{
	Super::BeginPlay();
	
	// 0.1초 뒤에 솟아오르는 함수 실행
	GetWorldTimerManager().SetTimer(EruptTimer, this, &AMageSkill2Rock::Erupt, 0.1f, false);
	// 2초 뒤에 알아서 사라짐
	SetLifeSpan(2.0f);
	
}

void AMageSkill2Rock::Erupt()
{
	OnStartEruption();
	
	if (NiagaraComp)
	{
		NiagaraComp->Activate(true);
	}
	
	// 2. 범위 내 대상 감지
	if (HasAuthority())
	{
		TArray<AActor*> OverlappingActors;
		OverlapComp->GetOverlappingActors(OverlappingActors);
	
		for (AActor* Actor : OverlappingActors)
		{
			ACharacter* TargetChar = Cast<ACharacter>(Actor);
		
			// 캐릭터가 아니거나 이미 죽었으면 무시
			if (!TargetChar) continue;
		
			ABaseCharacter* BaseChar = Cast<ABaseCharacter>(TargetChar);
			if (BaseChar && BaseChar->IsDead()) continue;
			
			// 판정 로직
			bool bIsEnemy = false;
			// 시전자가 있으면 적군 판별
			if (CasterPawn)
			{
				if (TargetChar != CasterPawn && !TargetChar->ActorHasTag("Player"))
				{
					bIsEnemy = true;
				}
			}
		
			if (bIsEnemy)
			{
				UGameplayStatics::ApplyDamage(TargetChar, SkillDamage,
					CasterPawn ? CasterPawn->GetController() : nullptr,
					this,
					UDamageType::StaticClass());
			}
			
			// AI(몬스터)가 안 뜨는 문제 해결용: 강제 낙하 모드 전환
			if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
			{
				CMC->StopMovementImmediately();     // 이동 멈춤
				CMC->SetMovementMode(MOVE_Falling); // 강제 낙하 모드 (중력 영향 받음)
			}
		
			// 에어본 (적, 아군, 본인 모두 띄움)
			FVector LaunchDir = FVector (0,0,1); // 수직
			// XY축을 힘을 0으로 덮어쓰고, Z축 힘을 덮어씀
			TargetChar-> LaunchCharacter(LaunchDir * 800.f, true, true);
	    }
	}
}

