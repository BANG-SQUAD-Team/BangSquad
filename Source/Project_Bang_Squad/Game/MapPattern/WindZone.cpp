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
        OnRep_IsGusting(); 
        
        TArray<AActor*> OverlappingActors;
        WindBox->GetOverlappingActors(OverlappingActors);
        
        for (AActor* Actor : OverlappingActors)
        {
            if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
            {
                // [수정] 통합 복구 함수 호출
                ResetCharacterState(Char);
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
            WarningWidgetInstance->RemoveFromParent();
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
    
    if (!bIsGusting)
    {
        // 구역 안에 있는 모든 캐릭터를 찾아서 초기화
        TArray<AActor*> OverlappingActors;
        if (WindBox)
        {
            WindBox->GetOverlappingActors(OverlappingActors);
            for (AActor* Actor : OverlappingActors)
            {
                if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
                {
                    // 클라이언트에서 직접 자신의 이동 수치 복구
                    ResetCharacterState(Char);
                }
            }
        }
    }
}

void AWindZone::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (!bIsGusting) return;
    
    TArray<AActor*> OverlappingActors;
    WindBox->GetOverlappingActors(OverlappingActors);
    
    if (OverlappingActors.Num() == 0) return;
    
    FVector WindForceDir = GetActorForwardVector() * (bPushForward ? 1.0f : -1.0f);
    FVector LookAtWindDir = -WindForceDir; 

    for (AActor* Actor : OverlappingActors)
    {
        ABaseCharacter* TargetChar = Cast<ABaseCharacter>(Actor);
        if (!TargetChar || !TargetChar->ActorHasTag("Player") || TargetChar->IsDead()) continue;
        
        bool bIsProtected = false;
        
        // 캡슐 정보
        FVector ActorLoc = TargetChar->GetActorLocation();
        float CapHalfHeight = 88.0f; 
        if (UCapsuleComponent* Cap = TargetChar->GetCapsuleComponent())
        {
            CapHalfHeight = Cap->GetScaledCapsuleHalfHeight();
        }

        // 체크 포인트 (허리, 무릎, 가슴)
        TArray<FVector> CheckPoints;
        CheckPoints.Add(ActorLoc);                                     
        CheckPoints.Add(ActorLoc - FVector(0, 0, CapHalfHeight * 0.5f)); 
        CheckPoints.Add(ActorLoc + FVector(0, 0, CapHalfHeight * 0.5f)); 

        FCollisionQueryParams Params;
        Params.AddIgnoredActor(TargetChar); // 내 몸은 무시 (필수!)
        Params.bTraceComplex = false; 

        float SphereRadius = 30.0f;
        FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

        for (const FVector& BasePoint : CheckPoints)
        {
            // 🚨 [수정 1] 시작점을 밖으로 빼지 말고, 캐릭터 중심에서 시작!
            // 그래야 벽에 딱 붙었을 때도 벽을 감지할 수 있음.
            FVector StartPoint = BasePoint; 
            FVector EndPoint = StartPoint + (LookAtWindDir * 450.0f); // 거리 넉넉하게

            FHitResult HitResult;

            if (GetWorld()->SweepSingleByChannel(HitResult, StartPoint, EndPoint, FQuat::Identity, ECC_WorldStatic, SphereShape, Params))
            {
                // 바닥 무시 (수직에 가까운 벽만 인정)
                if (HitResult.ImpactNormal.Z > 0.6f) continue; 

                bIsProtected = true;
                break; 
            }
        }

        if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
        {
            if (bIsProtected)
            {
                ResetCharacterState(TargetChar);
            }
            else
            {
                // [바람 상태]
                
                // 탈출을 위한 최소한의 제어력
                CMC->MaxAcceleration = 200.0f;            
                CMC->AirControl = 0.2f;

                CMC->MaxWalkSpeed = 6000.0f;            
                CMC->GroundFriction = 0.0f;             
                CMC->BrakingDecelerationWalking = 0.0f; 
                
                if (CMC->MovementMode == MOVE_Walking)
                {
                    TargetChar->LaunchCharacter(FVector(0.f, 0.f, 150.f), false, false);
                }

                float ForceMultiplier = 3000.0f; 
                FVector PushForce = WindForceDir * WindStrength * ForceMultiplier;
                PushForce.Z = 0.0f; 

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
        // 통합 복구 함수 호출
        ResetCharacterState(BaseChar);
    }
    
    if (OtherActor == UGameplayStatics::GetPlayerPawn(this,0))
    {
        UpdateWarningUI(false);
    }
}

// =========================================================================
// 캐릭터 상태 완전 초기화 함수 (새로 추가)
// =========================================================================
void AWindZone::ResetCharacterState(ABaseCharacter* TargetChar)
{
    if (!TargetChar) return;
    
    if (UCharacterMovementComponent* CMC = TargetChar->GetCharacterMovement())
    {
        // 1. 이동 수치 원상복구
        CMC->MaxWalkSpeed = TargetChar->GetDefaultWalkSpeed();
        CMC->MaxAcceleration = 2048.0f;       // 필수: 0이면 움직이지 않음
        CMC->GroundFriction = 8.0f;           // 필수: 0이면 멈추지 않음 (미끄러짐)
        CMC->BrakingDecelerationWalking = 2048.0f;
        CMC->AirControl = 0.05f;              // 기본 공중 제어값으로 복구
        
        // // 2. 물리 힘 초기화 (남아있는 바람의 힘 제거)
        // CMC->Velocity = FVector::ZeroVector;  // 선택사항: 관성을 즉시 없애고 싶다면 추가
        
        // 3. [중요] 울타리에 껴서 'Falling' 상태로 굳는 것 방지
        // 공중에 떠 있더라도 강제로 'Walking' 모드로 전환 시도
        if (CMC->MovementMode == MOVE_Falling)
        {
            CMC->SetMovementMode(MOVE_Walking);
        }
    }
}