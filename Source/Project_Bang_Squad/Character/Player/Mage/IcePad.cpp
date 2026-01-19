// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Character/Player/Mage/IcePad.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

AIcePad::AIcePad()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	
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
	if (!HasAuthority() || !OtherActor || OtherActor == GetInstigator()) return;

	// A. 플레이어라면? -> 아까 만든 함수 호출 (렉 방지)
	if (ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor))
	{
		if (!AffectedPlayers.Contains(Player))
		{
			Player->ApplySlowDebuff(true, SlowPercentage);
			AffectedPlayers.Add(Player);
		}
		return;
	}

	// B. 몬스터라면? -> 직접 조작 (코드 수정 불필요)
	if (ACharacter* Enemy = Cast<ACharacter>(OtherActor))
	{
		// 이미 기록된 놈이면 패스
		if (EnemyOriginalSpeeds.Contains(Enemy)) return;

		UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement();
		if (MoveComp)
		{
			// 원래 속도 장부에 적기
			EnemyOriginalSpeeds.Add(Enemy, MoveComp->MaxWalkSpeed);
			// 속도 깎기
			MoveComp->MaxWalkSpeed *= SlowPercentage;
		}
	}
}
void AIcePad::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// A. 플레이어
	if (ABaseCharacter* Player = Cast<ABaseCharacter>(OtherActor))
	{
		if (AffectedPlayers.Contains(Player))
		{
			Player->ApplySlowDebuff(false, SlowPercentage);
			AffectedPlayers.Remove(Player);
		}
		return;
	}

	// B. 몬스터
	if (ACharacter* Enemy = Cast<ACharacter>(OtherActor))
	{
		// 장부에 있니?
		if (EnemyOriginalSpeeds.Contains(Enemy))
		{
			// 장부에서 원래 속도 꺼내서 복구
			float SavedSpeed = EnemyOriginalSpeeds[Enemy];
			if (Enemy->GetCharacterMovement())
			{
				Enemy->GetCharacterMovement()->MaxWalkSpeed = SavedSpeed;
			}
			EnemyOriginalSpeeds.Remove(Enemy);
		}
	}
}

// 3. 장판 사라질 때 (뒷정리)
void AIcePad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 플레이어들 복구
	for (ABaseCharacter* Player : AffectedPlayers)
	{
		if (IsValid(Player)) Player->ApplySlowDebuff(false, SlowPercentage);
	}
	AffectedPlayers.Empty();

	// 몬스터들 복구 (장부 털기)
	for (auto& Elem : EnemyOriginalSpeeds)
	{
		ACharacter* Enemy = Elem.Key;
		float SavedSpeed = Elem.Value;

		if (IsValid(Enemy) && Enemy->GetCharacterMovement())
		{
			Enemy->GetCharacterMovement()->MaxWalkSpeed = SavedSpeed;
		}
	}
	EnemyOriginalSpeeds.Empty();
}