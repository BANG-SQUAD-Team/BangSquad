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
#include "Kismet/KismetMathLibrary.h" 
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"


AWindZone::AWindZone()
{
    PrimaryActorTick.bCanEverTick = true;
    
    WindBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WindBox"));
    RootComponent = WindBox;
    WindBox->SetBoxExtent(FVector(500.f,500.f,200.f));
    WindBox->SetCollisionProfileName(TEXT("Trigger"));

    WindVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindVFX"));
    WindVFX->SetupAttachment(RootComponent);
    
    ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComp"));
    ArrowComp->SetupAttachment(RootComponent);
    ArrowComp->ArrowSize = 5.0f;
}

void AWindZone::BeginPlay()
{
    Super::BeginPlay();

    if (WindEffectTemplate && SpawnInterval > 0.0f)
    {
       GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AWindZone::SpawnRandomWindVFX, SpawnInterval, true);
    }
}

void AWindZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!HasAuthority()) return;

    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors); 
    
    FVector WindDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    
    for (AActor* Actor : OverlappingActors)
    {
        ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Actor);
        if (!TargetChar || !TargetChar->ActorHasTag("Player") || TargetChar->IsDead()) continue;

        bool bIsProtected = false;

        // [Step 1] 보호 판정 (방패 및 차폐물)
        if (APaladinCharacter* Paladin = Cast<APaladinCharacter>(TargetChar))
        {
            if (Paladin->IsBlockingDirection(WindDir))
            {
                bIsProtected = true; 
                Paladin->ConsumeShield(DeltaTime * ShieldDamagePerSec); 
            }
        }
        
        if (!bIsProtected)
        {
            FVector TraceStart = TargetChar->GetActorLocation() + FVector(0.f, 0.f, 120.f);
            FVector TraceEnd = TraceStart - (WindDir * 250.0f);
            FHitResult HitResult;
            if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, BlockChannel, FCollisionQueryParams::DefaultQueryParam))
            {
                bIsProtected = true;
            }
        }

        // [Step 2] 물리 적용
        if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
        {
            if (bIsProtected)
            {
                TargetChar->bIsWindFloating = false;
                // ✅ 방패 안: 즉시 정상화 (툭 떨어짐)
                CMC->GravityScale = 1.0f;
                CMC->MaxWalkSpeed = TargetChar->GetDefaultWalkSpeed();
                
                // 조작 제한 해제 (속도가 0으로 고정되었다면 다시 입력을 받도록 함)
                if (CMC->MovementMode == MOVE_Falling && !CMC->IsFalling())
                {
                    CMC->SetMovementMode(MOVE_Walking);
                }
            }
            else
            {
                
                // 1. 앞으로 나가지 못하게 속도 고정
                // XY(수평) 속도를 매 프레임 0으로 죽여서 조작 입력을 무시합니다.
                CMC->Velocity.X = 0.0f;
                CMC->Velocity.Y = 0.0f;
                CMC->MaxWalkSpeed = 0.0f; // 입력에 의한 가속 차단

                // 2. 부유 높이 체크
                FVector Start = TargetChar->GetActorLocation();
                FVector End = Start - FVector(0, 0, 200.f);
                FHitResult FloorHit;
                GetWorld()->LineTraceSingleByChannel(FloorHit, Start, End, ECC_Visibility);
                float CurrentHeight = FloorHit.bBlockingHit ? FloorHit.Distance : 200.f;

                // 3. 부유 상태 설정
                if (CMC->MovementMode != MOVE_Falling) 
                {
                    CMC->SetMovementMode(MOVE_Falling);
                    // 땅에서 걷다가 뜰 때, 아주 살짝 위로 튕겨주어 '바닥 충돌'을 강제로 끊어줍니다.
                    TargetChar->LaunchCharacter(FVector(0, 0, 10.f), false, false); 
                }
                TargetChar->bIsWindFloating = true;
                CMC->GravityScale = 0.0f;

                // 4. 특정 높이(MaxHoverHeight) 유지 로직
                // 지정된 높이보다 낮으면 살짝 위로, 높으면 서서히 멈춤
                float TargetHoverHeight = 80.0f; 
                if (CurrentHeight < TargetHoverHeight)
                {
                    // 위로 슬슬 떠오르는 속도 주입
                    CMC->Velocity.Z = FMath::Lerp(CMC->Velocity.Z, 150.0f, DeltaTime * 5.0f);
                }
                else
                {
                    // 목표 높이에 도달하면 수직 속도를 서서히 줄여서 제자리에 고정
                    CMC->Velocity.Z = FMath::Lerp(CMC->Velocity.Z, 0.0f, DeltaTime * 3.0f);
                }
            }
        }
    }
}

void AWindZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);
    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
    {
       BaseChar->SetWindResistance(true); 
    }
}

void AWindZone::NotifyActorEndOverlap(AActor* OtherActor)
{
    Super::NotifyActorEndOverlap(OtherActor);

    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
    {
       BaseChar->SetWindResistance(false); 
       
       if (UCharacterMovementComponent* CMC = BaseChar->GetCharacterMovement())
       {
           CMC->GroundFriction = 8.0f;
           CMC->BrakingDecelerationWalking = 2048.0f;
           
           // 나갈 때도 캐릭터 고유의 속도로 완벽 복구
           CMC->MaxWalkSpeed = BaseChar->GetDefaultWalkSpeed();
       }
    }
}

void AWindZone::SpawnRandomWindVFX()
{
    if (!WindBox) return;

    FVector Center = WindBox->GetComponentLocation();      
    FVector Extent = WindBox->GetScaledBoxExtent();        
    FVector Forward = WindBox->GetForwardVector();         
    FVector Right = WindBox->GetRightVector();             
    FVector Up = WindBox->GetUpVector();                   

    FVector RandomOffset = 
       (Forward * FMath::RandRange(-Extent.X, Extent.X)) +
       (Right   * FMath::RandRange(-Extent.Y, Extent.Y)) +
       (Up      * FMath::RandRange(-Extent.Z, Extent.Z));

    FVector FinalSpawnLocation = Center + RandomOffset;

    if (WindEffectTemplate)
    {
       UNiagaraFunctionLibrary::SpawnSystemAtLocation(
          GetWorld(),
          WindEffectTemplate,
          FinalSpawnLocation, 
          GetActorRotation(), 
          FVector(3.0f),
          true,
          true,
          ENCPoolMethod::None,
          true
       );
    }
}