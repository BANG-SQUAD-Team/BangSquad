#include "Project_Bang_Squad/Character/StageBoss/BossSplitPattern.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "NiagaraFunctionLibrary.h"


ABossSplitPattern::ABossSplitPattern()
{
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// [수정] 거리를 양옆으로 1000씩(총 20미터) 넓힙니다.
	float ZoneDistance = 1000.f;

	// === A구역 설정 ===
	ZoneA_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneA_Mesh"));
	ZoneA_Mesh->SetupAttachment(RootComponent);
	ZoneA_Mesh->SetRelativeLocation(FVector(0.f, -ZoneDistance, 0.f));

	ZoneA_Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneA_Trigger"));
	ZoneA_Trigger->SetupAttachment(ZoneA_Mesh);
	ZoneA_Trigger->SetBoxExtent(FVector(200.f, 200.f, 200.f)); // 트리거 크기도 2배로!

	ZoneA_Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ZoneA_Text"));
	ZoneA_Text->SetupAttachment(ZoneA_Mesh);
	ZoneA_Text->SetRelativeLocation(FVector(0.f, 0.f, 300.f)); // 좀 더 높게
	ZoneA_Text->SetWorldSize(150.f); // [핵심] 글자 크기를 거대하게 키움
	ZoneA_Text->SetHorizontalAlignment(EHTA_Center); // 가운데 정렬
	ZoneA_Text->SetTextRenderColor(FColor::Cyan); // 눈에 띄는 색상

	// === B구역 설정 ===
	ZoneB_Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneB_Mesh"));
	ZoneB_Mesh->SetupAttachment(RootComponent);
	ZoneB_Mesh->SetRelativeLocation(FVector(0.f, ZoneDistance, 0.f));

	ZoneB_Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneB_Trigger"));
	ZoneB_Trigger->SetupAttachment(ZoneB_Mesh);
	ZoneB_Trigger->SetBoxExtent(FVector(200.f, 200.f, 200.f));

	ZoneB_Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ZoneB_Text"));
	ZoneB_Text->SetupAttachment(ZoneB_Mesh);
	ZoneB_Text->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	ZoneB_Text->SetWorldSize(150.f);
	ZoneB_Text->SetHorizontalAlignment(EHTA_Center);
	ZoneB_Text->SetTextRenderColor(FColor::Orange);
}

void ABossSplitPattern::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ZoneA_Trigger->OnComponentBeginOverlap.AddDynamic(this, &ABossSplitPattern::OnOverlapUpdate);
		ZoneA_Trigger->OnComponentEndOverlap.AddDynamic(this, &ABossSplitPattern::OnEndOverlapUpdate);

		ZoneB_Trigger->OnComponentBeginOverlap.AddDynamic(this, &ABossSplitPattern::OnOverlapUpdate);
		ZoneB_Trigger->OnComponentEndOverlap.AddDynamic(this, &ABossSplitPattern::OnEndOverlapUpdate);

		// [핵심] 서버에서 생성되자마자 스스로 바닥을 찾아 Z좌표를 내립니다.
		SnapToGround();
	}
}

void ABossSplitPattern::SnapToGround()
{
	if (!HasAuthority()) return;

	FVector StartLoc = GetActorLocation();
	FVector EndLoc = StartLoc - FVector(0.0f, 0.0f, 5000.0f); // 5000 유닛 아래로 레이캐스트

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	// 바닥(WorldStatic)만 감지하도록 설정
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

	bool bHit = GetWorld()->LineTraceSingleByObjectType(HitResult, StartLoc, EndLoc, ObjectParams, Params);

	if (bHit)
	{
		SetActorLocation(HitResult.ImpactPoint); // 닿은 바닥으로 즉시 이동
	}
}

void ABossSplitPattern::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossSplitPattern, RequiredA);
	DOREPLIFETIME(ABossSplitPattern, RequiredB);
}

void ABossSplitPattern::ActivatePattern(float PatternDuration)
{
	if (!HasAuthority()) return;

	int32 TotalPlayers = 0;
	if (GetWorld()->GetGameState())
	{
		TotalPlayers = GetWorld()->GetGameState()->PlayerArray.Num();
	}
	if (TotalPlayers < 1) TotalPlayers = 1;

	RequiredA = FMath::RandRange(0, TotalPlayers);
	RequiredB = TotalPlayers - RequiredA;

	OnRep_UpdateTexts();

	if (GEngine)
	{
		FString Msg = FString::Printf(TEXT("[패턴시작] 총원: %d명 -> A필요: %d / B필요: %d (제한시간: %.1f초)"),
			TotalPlayers, RequiredA, RequiredB, PatternDuration);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Purple, Msg);
	}

	// [핵심] 기존의 5.0f 대신, 보스가 알려준 PatternDuration(몽타주 길이)를 넣습니다!
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ABossSplitPattern::CheckResult, PatternDuration, false);
}

void ABossSplitPattern::OnRep_UpdateTexts()
{
	ZoneA_Text->SetText(FText::AsNumber(RequiredA));
	ZoneB_Text->SetText(FText::AsNumber(RequiredB));
}

void ABossSplitPattern::OnOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor);
	if (!Player || OtherComp != Player->GetRootComponent()) return;

	if (OverlappedComp == ZoneA_Trigger) CurrentA++;
	else if (OverlappedComp == ZoneB_Trigger) CurrentB++;
}

void ABossSplitPattern::OnEndOverlapUpdate(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor);
	if (!Player || OtherComp != Player->GetRootComponent()) return;

	if (OverlappedComp == ZoneA_Trigger) CurrentA--;
	else if (OverlappedComp == ZoneB_Trigger) CurrentB--;
}

void ABossSplitPattern::CheckResult()
{
	bool bSuccess = (CurrentA == RequiredA) && (CurrentB == RequiredB);

	// 보스에게 성공/실패 통보
	OnSplitPatternFinished.Broadcast(bSuccess);

	// 클라이언트들에게 화면 이펙트 재생 지시
	Multicast_PlayResultEffect(bSuccess);

	SetLifeSpan(1.5f);
}

void ABossSplitPattern::Multicast_PlayResultEffect_Implementation(bool bSuccess)
{
	if (GEngine)
	{
		FColor Color = bSuccess ? FColor::Green : FColor::Red;
		FString ResultStr = bSuccess ? TEXT("성공! (Success)") : TEXT("실패! (Fail)");
		FString Msg = FString::Printf(TEXT("[결과] %s\n - A구역: %d / %d\n - B구역: %d / %d"),
			*ResultStr, CurrentA, RequiredA, CurrentB, RequiredB);

		GEngine->AddOnScreenDebugMessage(-1, 5.0f, Color, Msg);
	}

	// 1. 결과에 맞는 이펙트 선택
	UNiagaraSystem* EffectToPlay = bSuccess ? SuccessVFX : FailVFX;

	// 2. 이펙트 재생 (기믹 장판의 정중앙에서 재생)
	if (EffectToPlay)
	{
		// [수정] 5번째 인자로 FVector(100.f)를 넣어주면 크기가 X, Y, Z 모두 100배가 됩니다!
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			EffectToPlay,
			GetActorLocation(),
			FRotator::ZeroRotator,
			FVector(100.0f) // <--- 바로 이 부분입니다!
		);
	}

	// 3. 이펙트가 터짐과 동시에, 원래 있던 장판과 글씨는 즉시 화면에서 숨깁니다.
	// (Destroy를 바로 해버리면 이펙트나 사운드 재생이 끊길 수 있으므로 숨기기만 합니다)
	if (ZoneA_Mesh) ZoneA_Mesh->SetVisibility(false, true);
	if (ZoneB_Mesh) ZoneB_Mesh->SetVisibility(false, true);
	if (ZoneA_Text) ZoneA_Text->SetVisibility(false);
	if (ZoneB_Text) ZoneB_Text->SetVisibility(false);
}