// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Character/Player/Mage/IcePad.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

AIcePad::AIcePad()
{
	PrimaryActorTick.bCanEverTick = false;
	
	// 충돌 박스 설정
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(FVector(200.f, 200.f, 100.f)); // 장판 크기
	CollisionBox->SetCollisionProfileName(TEXT("Trigger")); // 겹침 감지용
	
	// 데칼(바닥 그림) 설정
	IceDecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("IceDecalComp"));
	IceDecalComp->SetupAttachment(RootComponent);
	
	IceDecalComp->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	IceDecalComp->DecalSize = FVector(10.f, 200.f, 200.f); // X 가 두께임
	
	InitialLifeSpan = Duration; // 시간 지나면 자동 삭제

}


void AIcePad::BeginPlay()
{
	Super::BeginPlay();
	
	// 서버에서만 충돌 로직 처리 (데미지나 CC기는 서버 권한이 정석임)
	if (HasAuthority())
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AIcePad::OnOverlapBegin);
		CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AIcePad::OnOverlapEnd);
	}
	
}

void AIcePad::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ACharacter* TargetChar = Cast<ACharacter>(OtherActor);
	// 시전자는 제외하거나, 적군인지 확인하는 로직 추가 가능
	if (TargetChar && TargetChar != GetInstigator())
		{
			// 속도 감소
			TargetChar->GetCharacterMovement()->MaxWalkSpeed *= SlowPercentage;
		}
}

void AIcePad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	ACharacter* TargetChar = Cast<ACharacter>(OtherActor);
	if (TargetChar && TargetChar != GetInstigator())
		{
			TargetChar->GetCharacterMovement()->MaxWalkSpeed /= SlowPercentage;
		}
}

