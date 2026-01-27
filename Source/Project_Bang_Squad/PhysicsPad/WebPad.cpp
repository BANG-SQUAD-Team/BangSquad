#include "WebPad.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

AWebPad::AWebPad()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true; // 멀티플레이어 동기화 활성화

    // 1. 루트 컴포넌트 설정
    CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    RootComponent = CollisionBox;
    CollisionBox->SetBoxExtent(FVector(200.f, 200.f, 50.f));
    CollisionBox->SetCollisionProfileName(TEXT("Trigger"));

    // 1. 데칼 대신 스태틱 메쉬 생성
    WebMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WebMeshComp"));
    WebMeshComp->SetupAttachment(RootComponent);

    // 2. 메쉬 자체의 충돌은 끈다 (박스만 트리거로 사용)
    WebMeshComp->SetCollisionProfileName(TEXT("NoCollision"));
}

void AWebPad::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 물리 로직(속도 조절)을 수행합니다.
    if (HasAuthority())
    {
        CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AWebPad::OnOverlapBegin);
        CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AWebPad::OnOverlapEnd);
    }
}

void AWebPad::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (OtherActor) {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
            FString::Printf(TEXT("WebPad Overlap: %s"), *OtherActor->GetName()));
    }

    if (!HasAuthority() || !OtherActor) return;

    // A. 플레이어 감속
    if (ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor))
    {
        if (!AffectedPlayers.Contains(Player))
        {
            Player->ApplySlowDebuff(true, SlowPercentage);
            AffectedPlayers.Add(Player);
        }
        return;
    }

    // B. 일반 몬스터 감속
    if (ACharacter* Enemy = Cast<ACharacter>(OtherActor))
    {
        if (EnemyOriginalSpeeds.Contains(Enemy)) return;

        if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
        {
            EnemyOriginalSpeeds.Add(Enemy, MoveComp->MaxWalkSpeed);
            MoveComp->MaxWalkSpeed *= SlowPercentage;
        }
    }
}

void AWebPad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    // A. 플레이어 속도 복구
    if (ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor))
    {
        if (AffectedPlayers.Contains(Player))
        {
            Player->ApplySlowDebuff(false, SlowPercentage);
            AffectedPlayers.Remove(Player);
        }
        return;
    }

    // B. 일반 몬스터 속도 복구
    if (ACharacter* Enemy = Cast<ACharacter>(OtherActor))
    {
        if (EnemyOriginalSpeeds.Contains(Enemy))
        {
            if (Enemy->GetCharacterMovement())
            {
                Enemy->GetCharacterMovement()->MaxWalkSpeed = EnemyOriginalSpeeds[Enemy];
            }
            EnemyOriginalSpeeds.Remove(Enemy);
        }
    }
}

void AWebPad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 혹시라도 장판이 삭제될 때 밟고 있던 대상들 속도를 원복시킴 (안전장치)
    for (ABaseCharacter* Player : AffectedPlayers)
    {
        if (IsValid(Player)) Player->ApplySlowDebuff(false, SlowPercentage);
    }
    for (auto& Elem : EnemyOriginalSpeeds)
    {
        if (IsValid(Elem.Key) && Elem.Key->GetCharacterMovement())
        {
            Elem.Key->GetCharacterMovement()->MaxWalkSpeed = Elem.Value;
        }
    }
    Super::EndPlay(EndPlayReason);
}