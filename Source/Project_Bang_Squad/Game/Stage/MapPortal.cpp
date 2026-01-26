// Fill out your copyright notice in the Description page of Project Settings.


#include "MapPortal.h"

#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "Evaluation/IMovieSceneEvaluationHook.h"
#include "GameFramework/GameStateBase.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"

AMapPortal::AMapPortal()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
	RootComponent = TriggerSphere;
	TriggerSphere->SetSphereRadius(300.f);
	TriggerSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(RootComponent);
	PortalMesh->SetCollisionProfileName(TEXT("NoCollision"));

	CountdownText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CountdownText"));
	CountdownText->SetupAttachment(RootComponent);
	CountdownText->SetText(FText::GetEmpty());
	CountdownText->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	CountdownText->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
}

void AMapPortal::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance());
		if (GI && GI->GetbHasVisitedMap())
		{
			Destroy();
			return;
		}
		
		TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AMapPortal::OnOverlapBegin);
		TriggerSphere->OnComponentEndOverlap.AddDynamic(this, &AMapPortal::OnOverlapEnd);
	}
}

void AMapPortal::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	//BaseCharacter만 감지
	if (OtherActor && OtherActor->IsA(ABaseCharacter::StaticClass()))
	{
		OverlappingPlayers.Add(OtherActor);

		//GameState를 통해 플레이어 수 확인
		AGameStateBase* GS = GetWorld()->GetGameState();
		if (GS && OverlappingPlayers.Num() == GS->PlayerArray.Num())
		{
			StartCountdown();
		}
	}
}

void AMapPortal::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority()) return;
	if (OtherActor && OverlappingPlayers.Contains(OtherActor))
	{
		OverlappingPlayers.Remove(OtherActor);
		CancelCountdown();
	}
}

void AMapPortal::StartCountdown()
{
	RemainingTime = 5;

	//5초 후 이동
	GetWorldTimerManager().SetTimer(TravelTimerHandle, this, &AMapPortal::ProcessLevelTransition, 5.f, false);

	//1초마다 텍스트 갱신
	GetWorldTimerManager().SetTimer(CountdownUpdateTimer, this, &AMapPortal::UpdateCountdownText, 1.f, true);
	UpdateCountdownText();
}

void AMapPortal::CancelCountdown()
{
	if (GetWorldTimerManager().IsTimerActive(TravelTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(TravelTimerHandle);
		GetWorldTimerManager().ClearTimer(CountdownUpdateTimer);

		//텍스트 초기화
		MulticastUpdateText(TEXT("Canceled"));
	}
}

void AMapPortal::ProcessLevelTransition()
{
	UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance());
	if (GI)
	{
		GI->SetbHasVisitedMap(true);
	}

	FString LevelPath = TargetLevel.GetLongPackageName();

	if (!LevelPath.IsEmpty())
	{
		FString FinalPath = LevelPath + TEXT("?listen");
		GetWorld()->ServerTravel(FinalPath);
	}
}

void AMapPortal::UpdateCountdownText()
{
	if (RemainingTime > 0)
	{
		MulticastUpdateText(FString::FromInt(RemainingTime));
		RemainingTime--;
	}
}

void AMapPortal::MulticastUpdateText_Implementation(const FString& NewText)
{
	if (CountdownText)
		CountdownText->SetText(FText::FromString(NewText));
}
