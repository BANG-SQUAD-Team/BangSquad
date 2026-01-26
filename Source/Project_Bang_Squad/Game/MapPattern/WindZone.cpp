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
    
    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors, ABaseCharacter::StaticClass()); 
    
    FVector WindDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    
    for (AActor* Actor : OverlappingActors)
    {
        ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Actor);
        if (!TargetChar) continue;
        if (!TargetChar->ActorHasTag("Player")) continue; 

        // =========================================================
        // [1] 보호 여부 판단
        // =========================================================
        bool bIsProtected = false;

        // A. 팔라딘 본인 체크
        if (APaladinCharacter* Paladin = Cast<APaladinCharacter>(TargetChar))
        {
            if (Paladin->IsBlockingDirection(WindDir))
            {
                bIsProtected = true; 
                Paladin->ConsumeShield(DeltaTime * ShieldDamagePerSec); 
            }
        }

        // B. 다른 캐릭터 체크 (앞에 장애물/방패 있나 확인)
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

        // =========================================================
        // [2] 물리 상태 적용
        // =========================================================
        if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
        {
            if (bIsProtected)
            {
                //  [보호 상태] -> 원래대로 복구
                CMC->GroundFriction = 8.0f; 
                CMC->BrakingDecelerationWalking = 2048.0f;
                
                //  550.0f 대신 캐릭터 고유의 속도로 복구
                CMC->MaxWalkSpeed = TargetChar->GetDefaultWalkSpeed(); 
                
                // 보호받으면 바람 힘은 적용 안 함
            }
            else
            {
                //[피격 상태] -> 바람에 밀리는 세팅
                
                // 1. 앞으로 갈 때 저항감 (느리게)
                CMC->MaxWalkSpeed = 200.0f; 
                
                // 2. 약간 미끄럽게 (마찰 감소)
                CMC->GroundFriction = 2.0f; 
                
                // 3. 제동력 제거 (멈추면 바로 밀리게)
                CMC->BrakingDecelerationWalking = 0.0f;

                // 4. 바람 힘 적용
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