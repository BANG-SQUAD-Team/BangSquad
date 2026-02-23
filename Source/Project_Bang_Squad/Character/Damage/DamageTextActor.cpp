#include "DamageTextActor.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"

ADamageTextActor::ADamageTextActor()
{
	// 위로 스르륵 올라가야 하므로 Tick을 켭니다.
	PrimaryActorTick.bCanEverTick = true;

	DamageWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageWidget"));
	RootComponent = DamageWidgetComp;

	//  3D 월드 공간이 아니라 모니터 '화면(Screen)' 공간에 띄웁니다
	DamageWidgetComp->SetWidgetSpace(EWidgetSpace::Screen); 
	DamageWidgetComp->SetDrawSize(FVector2D(200.0f, 50.0f));

	// 액터가 생성된 후 1.5초 뒤에 스스로 메모리에서 삭제됩니다. (최적화)
	InitialLifeSpan = 1.0f; 
}

void ADamageTextActor::BeginPlay()
{
	Super::BeginPlay();

	// 여러 대를 동시에 맞았을 때 숫자가 겹치지 않게, 시작 위치를 살짝 랜덤으로 비틉니다.
	FVector RandomOffset = FVector(
		FMath::RandRange(-40.f, 40.f), 
		FMath::RandRange(-40.f, 40.f), 
		FMath::RandRange(0.f, 50.f)
	);
	AddActorLocalOffset(RandomOffset);
}

void ADamageTextActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 매 프레임마다 Z축(위)으로 올라갑니다. (초당 150의 속도)
	AddActorWorldOffset(FVector(0.f, 0.f, 50.0f * DeltaTime));
}

void ADamageTextActor::InitializeDamage(float DamageAmount)
{
	if (UUserWidget* WidgetObj = DamageWidgetComp->GetUserWidgetObject())
	{
		// 블루프린트에 만들어둘 "SetDamageText" 라는 이벤트를 호출합니다.
		UFunction* UpdateFunc = WidgetObj->FindFunction(FName("SetDamageText"));
		if (UpdateFunc)
		{
			struct { double Dmg; } Params;
			Params.Dmg = (double)DamageAmount;
			WidgetObj->ProcessEvent(UpdateFunc, &Params);
		}
	}
}