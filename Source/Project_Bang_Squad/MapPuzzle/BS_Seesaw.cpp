#include "BS_Seesaw.h"
#include "Components/BoxComponent.h"
#include "Components/ArrowComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

ABS_Seesaw::ABS_Seesaw()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 회전 중심점 설정
	PivotRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PivotRoot"));
	PivotRoot->SetupAttachment(RootComponent);

	// 가이드, 매쉬, 박스는 모두 PivotRoot의 자식으로! (매쉬 스케일 영향 방지)
	RotationAxisGuide = CreateDefaultSubobject<UArrowComponent>(TEXT("RotationAxisGuide"));
	RotationAxisGuide->SetupAttachment(PivotRoot);
	RotationAxisGuide->bIsEditorOnly = true;

	SeesawMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SeesawMesh"));
	SeesawMesh->SetupAttachment(PivotRoot);
}

void ABS_Seesaw::BeginPlay() { Super::BeginPlay(); }

void ABS_Seesaw::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABS_Seesaw, ServerCurrentRoll);
}

void ABS_Seesaw::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority()) // 서버에서만 계산
    {
        float TotalRotationForce = 0.f;
        bool bAnyPlayerOnSeesaw = false;

        // 1. 월드의 모든 캐릭터(또는 PlayerState)를 순회
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC) continue;

            // 캐릭터 가져오기
            ABaseCharacter* Character = Cast<ABaseCharacter>(PC->GetPawn());
            if (!Character || Character->IsDead()) continue;

            // 2. 캐릭터가 현재 이 시소의 Mesh를 밟고 있는지 확인
            if (Character->GetCharacterMovement()->GetMovementBase() == SeesawMesh)
            {
                bAnyPlayerOnSeesaw = true;

                // 3. 위치에 따른 힘 계산 (기존과 동일)
                FVector PivotLoc = PivotRoot->GetComponentLocation();
                FVector ArmDir = PivotRoot->GetRightVector();
                FVector Offset = Character->GetActorLocation() - PivotLoc;

                float DistanceForce = FVector::DotProduct(Offset, ArmDir);
                TotalRotationForce += DistanceForce;
            }
        }

        // 4. 회전 로직 적용
        if (!bAnyPlayerOnSeesaw)
        {
            // 아무도 없으면 복구
            if (!FMath::IsNearlyZero(ServerCurrentRoll, 0.1f))
            {
                float Direction = (ServerCurrentRoll > 0.f) ? -1.f : 1.f;
                ServerCurrentRoll += Direction * ReturnSpeed * DeltaTime;
                if ((Direction < 0.f && ServerCurrentRoll < 0.f) || (Direction > 0.f && ServerCurrentRoll > 0.f))
                    ServerCurrentRoll = 0.f;
            }
        }
        else
        {
            // 힘 누적
            ServerCurrentRoll += (TotalRotationForce * RotationSpeedMultiplier) * DeltaTime;
        }

        ServerCurrentRoll = FMath::Clamp(ServerCurrentRoll, -MaxAngle, MaxAngle);
    }

    // 화면 출력 (보간)
    ClientDisplayRoll = FMath::FInterpTo(ClientDisplayRoll, ServerCurrentRoll, DeltaTime, 10.0f);
    PivotRoot->SetRelativeRotation(FRotator(0.f, 0.f, ClientDisplayRoll));
}