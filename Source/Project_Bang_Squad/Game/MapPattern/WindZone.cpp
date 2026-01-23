// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Game/MapPattern/WindZone.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Character/PaladinCharacter.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h" // 랜덤 위치 계산용
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"


AWindZone::AWindZone()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	WindBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WindBox"));
	RootComponent = WindBox;
	WindBox->SetBoxExtent(FVector(500.f,500.f,200.f));
	WindBox->SetCollisionProfileName(TEXT("Trigger"));

	WindVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindVFX"));
	WindVFX->SetupAttachment(RootComponent);
	
	// 화살표 컴포넌트 추가
	ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComp"));
	ArrowComp->SetupAttachment(RootComponent);
    
	// 화살표 크기 좀 키우기 (잘 보이게)
	ArrowComp->ArrowSize = 5.0f;
}

void AWindZone::BeginPlay()
{
	Super::BeginPlay();

	// 이펙트가 있고 간격이 0보다 크면 타이머 시작
	if (WindEffectTemplate && SpawnInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AWindZone::SpawnRandomWindVFX, SpawnInterval, true);
	}
}

void AWindZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 1. 박스 안에 있는 캐릭터들 찾기
    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors, ACharacter::StaticClass());
    
    FVector WindDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    
    for (AActor* Actor : OverlappingActors)
    {
        ACharacter* TargetChar = Cast<ACharacter>(Actor);
        if (!TargetChar) continue;
        
        bool bIsProtected = false; // 기본값: 보호 못 받음 (날아감)

    	if (!TargetChar->ActorHasTag("Player"))
    	{
    		continue; 
    	}
    	
        // =========================================================
        // [1] 팔라딘 본인 체크
        // "팔라딘이 방어 중이고 + 바람을 정면으로 보고 있을 때만" 보호됨
        // 그 외(방패 내림, 뒤돔, 옆돔)에는 얄짤없이 날아감
        // =========================================================
        if (APaladinCharacter* Paladin = Cast<APaladinCharacter>(TargetChar))
        {
            if (Paladin->IsBlockingDirection(WindDir))
            {
                bIsProtected = true; // 이때만 안 날아감
                Paladin->ConsumeShield(DeltaTime * ShieldDamagePerSec); // (옵션) 방패 체력 소모
            }
        }

        // =========================================================
        // [2] 다른 캐릭터들 (차폐 판정)
        // 팔라딘이 아니거나, 팔라딘인데 방어를 실패해서 날아가는 중이라면 체크할 필요 없음
        // =========================================================
        if (!bIsProtected)
        {
            FVector TraceEnd = TargetChar->GetActorLocation();
            FVector TraceStart = TraceEnd - (WindDir * 1000.0f); 
            
            FHitResult HitResult;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(this);
            Params.AddIgnoredActor(TargetChar); // 나 자신 무시
            
            bool bHit = GetWorld()->LineTraceSingleByChannel(
                HitResult, TraceStart, TraceEnd, ECC_Visibility, Params
            );
            
            if (bHit)
            {
                // 내 앞에 벽이나 방패가 있으면 보호됨
                bIsProtected = true;
            }
        }
        
        // =========================================================
        // [3] 결과 적용: 보호 못 받았으면 날려버림
        // =========================================================
        if (!bIsProtected)
        {
            if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(TargetChar->GetMovementComponent()))
            {
               // 아까 만든 "땅 마찰력 무시하고 밀어버리는" 로직
               float FinalStrength = WindStrength;
               if (CMC->IsMovingOnGround())
               {
                   FinalStrength *= GroundFrictionMultiplier;
               }
               CMC->AddForce(WindDir * FinalStrength);
            }
        }
    }
}

// 1. 들어올 때 브레이크 풀기
void AWindZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
	{
		BaseChar->SetWindResistance(true); // 브레이크 해제!
	}
}

void AWindZone::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
	{
		BaseChar->SetWindResistance(false); // 브레이크 복구!
	}
}

void AWindZone::SpawnRandomWindVFX()
{
	if (!WindBox) return;

	// 1. 박스의 현재 정보 가져오기
	FVector Center = WindBox->GetComponentLocation();       // 박스 중심 (월드 좌표)
	FVector Extent = WindBox->GetScaledBoxExtent();         // 박스 크기 (반지름 개념)
	FVector Forward = WindBox->GetForwardVector();          // 박스 앞쪽 방향
	FVector Right = WindBox->GetRightVector();              // 박스 오른쪽 방향
	FVector Up = WindBox->GetUpVector();                    // 박스 위쪽 방향

	// 2. [핵심] 중심에서 각 방향으로 랜덤한 거리만큼 이동시키기
	// (회전된 상태의 방향 벡터를 쓰기 때문에, 박스가 돌아가 있어도 정확히 그 안으로 들어감)
	FVector RandomOffset = 
		(Forward * FMath::RandRange(-Extent.X, Extent.X)) +
		(Right   * FMath::RandRange(-Extent.Y, Extent.Y)) +
		(Up      * FMath::RandRange(-Extent.Z, Extent.Z));

	FVector FinalSpawnLocation = Center + RandomOffset;

	// 3. 디버그 (빨간 공): 이게 박스 밖으로 나가면 제 손에 장을 지지겠습니다.
	// DrawDebugSphere(GetWorld(), FinalSpawnLocation, 15.0f, 12, FColor::Red, false, 0.5f);

	// 4. 나이아가라 생성
	if (WindEffectTemplate)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			WindEffectTemplate,
			FinalSpawnLocation, // 계산된 정확한 위치
			GetActorRotation(), // 회전값은 윈드존 따라가기
			FVector(3.0f),
			true,
			true,
			ENCPoolMethod::None,
			true
		);
	}
}