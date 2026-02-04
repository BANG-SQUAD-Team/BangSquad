#include "WebPad.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

AWebPad::AWebPad()
{
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;

    CollisionBox->SetCollisionProfileName(TEXT("Trigger"));
    //CollisionBox->SetCollisionResponseToAllChannels(ECR_Block);
    //CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    CollisionBox->SetGenerateOverlapEvents(true);
	
    WebMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    WebMeshComp->SetupAttachment(RootComponent);
    WebMeshComp->SetCollisionProfileName(TEXT("NoCollision"));

    CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AWebPad::OnOverlapBegin);
    CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AWebPad::OnOverlapEnd);
}

void AWebPad::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ABaseCharacter* Character = Cast<ABaseCharacter>(OtherActor);
    if (Character && Character->GetCharacterMovement())
    {
        //이속 감소
        Character->ApplySlowDebuff(true, SpeedMultiplier);

        //중력 감소
        Character->GetCharacterMovement()->GravityScale = WebGravityScale;

        //점프 봉인
        Character->SetJumpRestricted(true);

        Character->GetCharacterMovement()->GroundFriction = 20.0f;

        Character->GetCharacterMovement()->BrakingDecelerationWalking = 10000.0f;

        //공중 마찰력
        Character->GetCharacterMovement()->FallingLateralFriction = 100.f;
		
    }
}

void AWebPad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    ABaseCharacter* Character = Cast<ABaseCharacter>(OtherActor);
    if (Character && Character->GetCharacterMovement())
    {
        Character->ApplySlowDebuff(false, 1.f);

        Character->GetCharacterMovement()->GravityScale = 1.0f;

        //점프력 복구
        Character->SetJumpRestricted(false);

        Character->GetCharacterMovement()->FallingLateralFriction = 0.f;

        Character->GetCharacterMovement()->GroundFriction = 8.0f; 
        Character->GetCharacterMovement()->BrakingDecelerationWalking = 2048.0f;
    }
}

float AWebPad::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    class AController* EventInstigator, AActor* DamageCauser)
{
    if (!bDestroyAvailable) return 0.f;
    
    if (DamageAmount > 0.f)
    {
        Destroy();
    }
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}