#include "BreakingPad.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

ABreakingPad::ABreakingPad()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    AActor::SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
}

void ABreakingPad::BeginPlay()
{
    Super::BeginPlay();

    // 액터에 붙은 모든 스태틱 매쉬를 찾아 초기 설정 수행
    TArray<UStaticMeshComponent*> Components;
    GetComponents<UStaticMeshComponent>(Components);

    for (UStaticMeshComponent* Piece : Components)
    {
        if (Piece)
        {
            Piece->SetMobility(EComponentMobility::Movable);
            Piece->SetNotifyRigidBodyCollision(true); // Hit 이벤트 활성화
            Piece->SetCollisionProfileName(TEXT("BlockAllDynamic"));
            Piece->SetGenerateOverlapEvents(true);

            if (HasAuthority())
            {
                Piece->OnComponentHit.RemoveDynamic(this, &ABreakingPad::OnPieceHit);
                Piece->OnComponentHit.AddDynamic(this, &ABreakingPad::OnPieceHit);
            }
        }
    }
}

void ABreakingPad::OnPieceHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 서버에서만 판정
    if (HasAuthority())
    {
        // 1. 캐릭터인지 확인
        ACharacter* Character = Cast<ACharacter>(OtherActor);
        if (!Character) return;

        // 2. 디버깅 로그 (어디를 밟았는지 서버 로그에 찍어봄)
        UE_LOG(LogTemp, Warning, TEXT("Hit Normal Z: %f"), Hit.Normal.Z);

        // 3. 판정
        if (Hit.Normal.Z < -0.2f)
        {
            UStaticMeshComponent* SteppedPiece = Cast<UStaticMeshComponent>(HitComp);

            // 이미 물리 시뮬레이션 중이면 무시
            if (SteppedPiece && !SteppedPiece->IsSimulatingPhysics())
            {
                SteppedPiece->SetNotifyRigidBodyCollision(false);

                // 타이머 및 드롭 로직 (기존 동일)
                FTimerHandle DropTimer;
                GetWorldTimerManager().SetTimer(DropTimer, [this, SteppedPiece]() {
                    Multicast_DropPiece(SteppedPiece);
                    }, DropDelay, false);
            }
        }
    }
}

void ABreakingPad::Multicast_DropPiece_Implementation(UStaticMeshComponent* PieceToDrop)
{
    if (PieceToDrop)
    {
        // 물리 및 중력 활성화
        PieceToDrop->SetSimulatePhysics(true);
        PieceToDrop->SetEnableGravity(true);

        // 하방 충격 가하기
        PieceToDrop->AddImpulse(FVector(0, 0, -10.f), NAME_None, true);

        // 5초 후 해당 컴포넌트 제거
        FTimerHandle CleanupHandle;
        GetWorldTimerManager().SetTimer(CleanupHandle, [PieceToDrop]() {
            if (IsValid(PieceToDrop))
            {
                PieceToDrop->DestroyComponent();
            }
            }, 5.0f, false);
    }
}