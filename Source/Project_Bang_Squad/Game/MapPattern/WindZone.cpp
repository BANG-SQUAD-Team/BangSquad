#include "Project_Bang_Squad/Game/MapPattern/WindZone.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h" 
#include "Net/UnrealNetwork.h" // 리플리케이션 필수 헤더
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"


AWindZone::AWindZone()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true; // 서버-클라 동기화 활성화
    
    WindBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WindBox"));
    RootComponent = WindBox;
    WindBox->SetBoxExtent(FVector(500.f,500.f,200.f));
    WindBox->SetCollisionProfileName(TEXT("Trigger"));

    WindVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WindVFX"));
    WindVFX->SetupAttachment(RootComponent);
    WindVFX->bAutoActivate = false; 
    
    ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrowComp"));
    ArrowComp->SetupAttachment(RootComponent);
    ArrowComp->ArrowSize = 5.0f;
}

void AWindZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AWindZone, bIsGusting); // bIsGusting 변수 동기화 등록
    DOREPLIFETIME(AWindZone, bIsWarning); 
}

void AWindZone::BeginPlay()
{
    Super::BeginPlay();
    
    // 시작할 때는 이펙트 끄기
    if (WindVFX) WindVFX->Deactivate();
    
    // 타이머는 서버만 관리
    if (HasAuthority())
    {
        bIsGusting = false;
        bIsWarning = false;
        GetWorldTimerManager().SetTimer(GustCycleTimer, this, &AWindZone::StartWarning, CalmDuration, false);
    }
}

void AWindZone::StartWarning()
{
    if (HasAuthority())
    {
        bIsWarning = true; // 클라에 전파 ->UI 켜짐
        OnRep_IsWarning();
        
        // WarningDuration 후에 바람(GUST) 시작
        GetWorldTimerManager().SetTimer(GustCycleTimer, this, &AWindZone::StartGust,
            WarningDuration, false);
    }
 
    
}

void AWindZone::StartGust()
{
    if (HasAuthority())
    {
        bIsWarning = false; // 경고 끄기
        OnRep_IsWarning();
        
        bIsGusting = true; // 바람 시작
        OnRep_IsGusting(); 
        
    
        GetWorldTimerManager().SetTimer(GustCycleTimer, this, &AWindZone::StopGust, 
            GustDuration, false);
    }
}

void AWindZone::StopGust()
{
    if (HasAuthority())
    {
        bIsGusting = false;
        OnRep_IsGusting(); // 서버에서도 이펙트 끄기
        
        // 바람이 멈췄으니 구역 내 플레이어들의 이동 능력을 복구
        TArray<AActor*> OverlappingActors;
        WindBox->GetOverlappingActors(OverlappingActors);
        for (AActor* Actor : OverlappingActors)
        {
            if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
            {
                if (UCharacterMovementComponent* CMC = Char->GetCharacterMovement())
                {
                    // [복구] 정상 이동 가능하도록 설정
                    CMC->MaxWalkSpeed = Char->GetDefaultWalkSpeed();
                    CMC->MaxAcceleration = 2048.0f; // 가속도 복구 (다시 움직일 수 있음)
                    CMC->GroundFriction = 8.0f;     // 마찰력 복구
                    CMC->BrakingDecelerationWalking = 2048.0f;
                }
            }
        }
        
        GetWorldTimerManager().SetTimer(GustCycleTimer, this, &AWindZone::StartWarning, CalmDuration, false);
    }
}

void AWindZone::OnRep_IsWarning()
{
    // 경고 상태가 켜졌는지 확인
    if (bIsWarning)
    {
        // 내가 이 구역 안에 있는지 확인
        APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        if (LocalPawn && WindBox ->IsOverlappingActor(LocalPawn))
        {
            UpdateWarningUI(true); // 안에 있으면 켬
        }
    }
    else
    {
        UpdateWarningUI(false);
    }
}

void AWindZone::UpdateWarningUI(bool bShow)
{
    // 로컬 클라이언트에서만 UI 처리
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !PC->IsLocalController()) return;
    
    if (bShow)
    {
        if (!WarningWidgetInstance && WarningWidgetClass)
        {
            WarningWidgetInstance = CreateWidget<UUserWidget>(PC, WarningWidgetClass);
        }
        
        if (WarningWidgetInstance && !WarningWidgetInstance->IsInViewport())
        {
            WarningWidgetInstance->AddToViewport();
        }
    }
    else
    {
        if (WarningWidgetInstance && WarningWidgetInstance->IsInViewport())
        {
            WarningWidgetInstance->RemoveFromViewport();
        }
    }
}

void AWindZone::OnRep_IsGusting()
{
    // 변수 값이 바뀌면 자동으로 이펙트 끄고 켜기
    if (WindVFX)
    {
        if (bIsGusting) WindVFX->Activate(true);
        else WindVFX->Deactivate();
    }
}

void AWindZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!bIsGusting) return;
    
    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors);
    
    if (OverlappingActors.Num() == 0) return;
    
    // 바람 방향 설정
    FVector WindForceDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    FVector LookAtWindDir = -WindForceDir; 

    for (AActor* Actor : OverlappingActors)
    {
        ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Actor);
        if (!TargetChar || !TargetChar->ActorHasTag("Player") || TargetChar->IsDead()) continue;
        
        bool bIsProtected = false;
        
        // 캡슐 정보 가져오기
        FVector ActorLoc = TargetChar->GetActorLocation();
        float CapHalfHeight = 88.0f; 
        float CapRadius = 34.0f;
        if (UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
        {
            CapHalfHeight = Cap->GetScaledCapsuleHalfHeight();
            CapRadius = Cap->GetScaledCapsuleRadius();
        }

        // 체크 포인트: 허리, 무릎, 가슴
        TArray<FVector> CheckPoints;
        CheckPoints.Add(ActorLoc);                                     
        CheckPoints.Add(ActorLoc - FVector(0, 0, CapHalfHeight * 0.5f)); 
        CheckPoints.Add(ActorLoc + FVector(0, 0, CapHalfHeight * 0.5f)); 

        // 트레이스 파라미터
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(TargetChar); 
        Params.bTraceComplex = false; // Simple Collision(박스)만 검사

        float SphereRadius = 30.0f;
        FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

        for (const FVector& BasePoint : CheckPoints)
        {
            // 시작점을 캐릭터 캡슐 밖으로 살짝 이동
            FVector StartPoint = BasePoint + (LookAtWindDir * (CapRadius + 20.0f));
            FVector EndPoint = StartPoint + (LookAtWindDir * 400.0f); 

            FHitResult HitResult;

            // ECC_WorldStatic 채널로 검사 (벽, 울타리 등)
            if (GetWorld()->SweepSingleByChannel(HitResult, StartPoint, EndPoint, FQuat::Identity, ECC_WorldStatic, SphereShape, Params))
            {
                // 바닥/경사면 판정 (Z값이 크면 바닥이므로 무시)
                if (HitResult.ImpactNormal.Z > 0.6f) 
                {
                    continue; 
                }

                // 벽이나 울타리에 막힘
                bIsProtected = true;
                break; 
            }
        }

        if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
        {
            if (bIsProtected)
            {
                // [보호 상태] : 정상 이동 가능
                CMC->MaxWalkSpeed = TargetChar->GetDefaultWalkSpeed();
                CMC->MaxAcceleration = 2048.0f; 
                CMC->GroundFriction = 8.0f;     
                CMC->BrakingDecelerationWalking = 2048.0f;
                
                // Falling 상태였다면 Walking으로 전환 (굳음 방지)
                if (CMC->MovementMode == MOVE_Falling)
                {
                    CMC->SetMovementMode(MOVE_Walking);
                }
            }
            else
            {
                // [바람 상태] : 뚫기 불가 + 뒤로 날아감
                CMC->MaxAcceleration = 0.0f;            // 입력 차단
                CMC->MaxWalkSpeed = 6000.0f;            // 속도 제한 해제
                CMC->GroundFriction = 0.0f;             // 마찰력 제거
                CMC->BrakingDecelerationWalking = 0.0f; // 제동력 제거
                
                // 땅에 붙어있으면 띄워서 마찰력 완전 제거
                if (CMC->MovementMode == MOVE_Walking)
                {
                    TargetChar->LaunchCharacter(FVector(0.f, 0.f, 150.f), false, false);
                }

                // 물리 힘 적용
                float ForceMultiplier = 3000.0f; 
                FVector PushForce = WindForceDir * WindStrength * ForceMultiplier;
                PushForce.Z = 0.0f; // 수평으로만 밀기

                CMC->AddForce(PushForce);   
            }
        }
    }
}

void AWindZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
    Super::NotifyActorBeginOverlap(OtherActor);
    
    if (OtherActor == UGameplayStatics::GetPlayerPawn(this,0))
    {
        if (bIsWarning)
        {
            UpdateWarningUI(true);
        }
    }
}

void AWindZone::NotifyActorEndOverlap(AActor* OtherActor)
{
    Super::NotifyActorEndOverlap(OtherActor);

    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(OtherActor))
    {
        if (UCharacterMovementComponent* CMC = BaseChar->GetCharacterMovement())
        {
            // 구역 나가면 즉시 정상 상태로 복구
            CMC->MaxWalkSpeed = BaseChar->GetDefaultWalkSpeed();
            CMC->MaxAcceleration = 2048.0f;
            CMC->GroundFriction = 8.0f;
            CMC->BrakingDecelerationWalking = 2048.0f;
        }
    }
    
    // UI 끄기
    if (OtherActor == UGameplayStatics::GetPlayerPawn(this,0))
    {
        UpdateWarningUI(false);
    }
}