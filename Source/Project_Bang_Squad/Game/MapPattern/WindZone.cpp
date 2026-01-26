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
    
    // [수정 1] 특정 클래스(ABaseCharacter)만 찾던 것을 -> 모든 액터(AActor)로 변경
    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors); 
    
    FVector WindDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    
    for (AActor* Actor : OverlappingActors)
    {
        if (!Actor || Actor == this) continue;

        // =========================================================
        // [CASE A] 캐릭터 처리 (기존 로직 유지)
        // =========================================================
        if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Actor))
        {
            // 플레이어 태그 체크 (기존 유지)
            if (!TargetChar->ActorHasTag("Player")) continue; 

            bool bIsProtected = false;

            // 1. 팔라딘 방어 체크
            if (APaladinCharacter* Paladin = Cast<APaladinCharacter>(TargetChar))
            {
                if (Paladin->IsBlockingDirection(WindDir))
                {
                    bIsProtected = true; 
                    Paladin->ConsumeShield(DeltaTime * ShieldDamagePerSec); 
                }
            }

            // 2. 차폐물(LineTrace) 체크
            if (!bIsProtected)
            {
                FVector TraceEnd = TargetChar->GetActorLocation();
                FVector TraceStart = TraceEnd - (WindDir * 800.0f); 
                
                FHitResult HitResult;
                FCollisionQueryParams Params;
                Params.AddIgnoredActor(this);
                Params.AddIgnoredActor(TargetChar); 
                
                bool bHit = GetWorld()->LineTraceSingleByChannel(
                    HitResult, TraceStart, TraceEnd, BlockChannel, Params
                );
                
                if (bHit) bIsProtected = true; 
            }

            // 3. 물리 적용 (캐릭터 무브먼트)
            if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
            {
                if (bIsProtected)
                {
                    CMC->GroundFriction = 8.0f; 
                    CMC->BrakingDecelerationWalking = 2048.0f;
                    CMC->MaxWalkSpeed = TargetChar->GetDefaultWalkSpeed(); 
                }
                else
                {
                    CMC->MaxWalkSpeed = 200.0f; 
                    CMC->GroundFriction = 2.0f; 
                    CMC->BrakingDecelerationWalking = 0.0f;

                    float FinalStrength = WindStrength;
                    if (CMC->IsMovingOnGround())
                    {
                        FinalStrength *= GroundFrictionMultiplier;
                    }
                    CMC->AddForce(WindDir * FinalStrength);
                }
            }
        }
        // =========================================================
        // [CASE B] 물리 액터 처리 (버섯 등) -> 새로 추가된 부분
        // =========================================================
        else if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
        {
            // "Simulate Physics"가 켜진 물체만 바람에 날아감
            if (PrimComp->IsSimulatingPhysics())
            {
                // 버섯은 가벼우니까 캐릭터보다 바람을 더 잘 타야 함
                // DeltaTime을 곱하지 않고 AddForce를 계속 주면 가속도가 붙어서 자연스럽게 날아감
                // 필요하다면 여기서 힘 조절 
                PrimComp->AddForce(WindDir * WindStrength);
                
                //  바람에 날릴 때 약간의 회전(Torque)을 주면 더 리얼함
                PrimComp->AddTorqueInRadians(FVector(0, 0, 100.0f));
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