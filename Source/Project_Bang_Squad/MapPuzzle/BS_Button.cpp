#include "BS_Button.h"
#include "BS_Door.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"


ABS_Button::ABS_Button()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
    BaseMesh->SetupAttachment(RootComponent);

    SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
    SwitchMesh->SetupAttachment(RootComponent);
}

void ABS_Button::BeginPlay()
{
    Super::BeginPlay();
    // 시작할 때 에디터에 배치된 진짜 위치를 기억
    InitialRelativeLocation = SwitchMesh->GetRelativeLocation();

    // 월드에서 나를 파트너로 등록한 마스터 버튼들을 찾아 목록에 넣음
    TArray<AActor*> AllButtons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABS_Button::StaticClass(), AllButtons);

    for (AActor* Actor : AllButtons)
    {
        ABS_Button* PotentialMaster = Cast<ABS_Button>(Actor);

        // 내가 아닌 다른 버튼들 중에서 찾습니다.
        if (PotentialMaster && PotentialMaster != this)
        {
            // 그 버튼의 파트너 리스트에 내가 들어있다면?
            if (PotentialMaster->PartnerButtons.Contains(this))
            {
                // 그 녀석이 바로 나의 주인님(MyMasters)입니다!
                MyMasters.Add(PotentialMaster);
            }
        }
    }
}

void ABS_Button::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority()) // 서버에서만 인원 체크
    {
        int32 NewCount = 0;
        // 시소에서 썼던 로직: 월드의 모든 캐릭터를 순회하며 나를 밟고 있는지 확인
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (APlayerController* PC = It->Get())
            {
                if (ABaseCharacter* Character = Cast<ABaseCharacter>(PC->GetPawn()))
                {
                    if (!Character->IsDead() && Character->GetCharacterMovement()->GetMovementBase() == SwitchMesh)
                    {
                        NewCount++;
                    }
                }
            }
        }

        // 인원수가 변했을 때만 조건 재계산
        if (NewCount != CurrentPlayerCount)
        {
            CurrentPlayerCount = NewCount;
            EvaluateTotalCondition();
        }
    }

    // 시각적 연출: 초기 위치에서 지정된 깊이만큼만 내려가게 수정
    float TargetAlpha = (CurrentPlayerCount > 0) ? 1.f : 0.f;
    VisualInterpAlpha = FMath::FInterpTo(VisualInterpAlpha, TargetAlpha, DeltaTime, 10.f);

    // 절대 좌표 0이 아니라 InitialRelativeLocation 기준으로 오프셋을 줍니다
    FVector TargetOffset = FVector(0.f, 0.f, VisualInterpAlpha * PressDepth);
    SwitchMesh->SetRelativeLocation(InitialRelativeLocation + TargetOffset);
}

void ABS_Button::EvaluateTotalCondition()
{
    bIsLocallyActivated = (CurrentPlayerCount >= RequiredPlayers);

    // 1. 만약 나를 부리는 '마스터'가 있다면, 판단을 마스터에게 맡기고 나는 보고만 함
    if (MyMasters.Num() > 0)
    {
        for (ABS_Button* Master : MyMasters)
        {
            if (Master) Master->EvaluateTotalCondition();
        }
        // 마스터가 있는 슬레이브 버튼은 직접 문을 조작하지 않음 (이게 4번이 문을 여는 걸 막아줌)
        return;
    }

    // 2. 내가 마스터이거나 솔로 버튼일 때만 아래 로직 실행
    bool bAllPartnersReady = true;
    for (ABS_Button* Partner : PartnerButtons)
    {
        if (Partner && !Partner->IsConditionMet())
        {
            bAllPartnersReady = false;
            break;
        }
    }

    if (TargetDoor)
    {
        if (bIsLocallyActivated && bAllPartnersReady)
        {
            TargetDoor->ExecuteAction(PressedAction);
        }
        else
        {
            TargetDoor->ExecuteAction(EDoorAction::Close);
        }
    }
}

void ABS_Button::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ABS_Button, CurrentPlayerCount);
}