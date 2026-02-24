
#include "Project_Bang_Squad/Character/Player/Mage/MageSkill2Rock.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Particles/ParticleSystemComponent.h"
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
	ParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("ParticleComp"));
	ParticleComp->SetupAttachment(RootComponent); // 바위 중심에 부착
    
	// 태어날 때는 꺼둠 (솟아오를 때 켤 것임)
	ParticleComp->bAutoActivate = false; 
    
	// 바위 메쉬보다 약간 아래 배치 (흙먼지 느낌)
	ParticleComp->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
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
	SetLifeSpan(5.0f);
	
}

void AMageSkill2Rock::Erupt()
{
	OnStartEruption();
	
	
	if (RockMesh)
	{
		RockMesh->SetVisibility(true);
		// 아직은 물리적으로 막지 않고 감지만 하도록 설정 (끼임 방지)
		RockMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		RockMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	}
	
	if (ParticleComp)
	{
		ParticleComp->SetRelativeScale3D(ParticleScale);
		
		ParticleComp->Activate(true);
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
			
			//  데미지는 '적'에게만 (플레이어 태그가 없으면 적으로 간주)
			bool bIsEnemy = !TargetChar->ActorHasTag(TEXT("Player"));
			// 시전자가 있으면 적군 판별
			
			if (bIsEnemy)
			{
				UGameplayStatics::ApplyDamage(TargetChar, SkillDamage,
					CasterPawn ? CasterPawn->GetController() : nullptr,
					this,
					UDamageType::StaticClass());
			}
			
			// 에어본은 적/아군/본인 가리지 않고 '모두' 띄움
			if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
			{
				CMC->StopMovementImmediately();
				CMC->SetMovementMode(MOVE_Falling);
			}
			
			//  수평 밀쳐내기 계산
			// 바위 중심에서 캐릭터 방향으로의 벡터를 구합니다.
			FVector RockLoc = GetActorLocation();
			FVector TargetLoc = TargetChar->GetActorLocation();
			FVector LaunchDir = TargetLoc - RockLoc;
			
			LaunchDir.Z = 0.0f; // 수평 방향만 남깁니다.
			LaunchDir.Normalize();
			
			// 수평으로 300, 수직으로 800의 힘을 합칩니다.
			FVector FinalLaunchImpulse = (LaunchDir * 300.f) + FVector(0.f, 0.f, 800.f);
			
			// 캐릭터를 밖+위로 튕겨냅니다.
			TargetChar->LaunchCharacter(FinalLaunchImpulse, true, true);
	    }
		
		//  0.2초 뒤에 바위를 딱딱한 벽(BlockAll)으로 바꿉니다.
		// 캐릭터들이 LaunchCharacter에 의해 바위 영역 밖으로 충분히 나간 뒤에 벽이 생기게 합니다.
		FTimerHandle BlockTimerHandle;
		GetWorldTimerManager().SetTimer(BlockTimerHandle, [this]()
		{
			if (IsValid(this) && RockMesh)
			{
				RockMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				RockMesh->SetCollisionProfileName(TEXT("BlockAll"));
			}
		}, 0.2f, false);
	}
}

