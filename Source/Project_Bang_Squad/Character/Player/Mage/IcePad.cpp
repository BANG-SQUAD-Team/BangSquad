// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Character/Player/Mage/IcePad.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"

AIcePad::AIcePad()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	
	// 충돌 박스 설정
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(FVector(350.f, 350.f, 100.f)); // 장판 크기
	CollisionBox->SetCollisionProfileName(TEXT("Trigger")); // 겹침 감지용
	
	// 데칼(바닥 그림) 설정
	IceDecalComp = CreateDefaultSubobject<UDecalComponent>(TEXT("IceDecalComp"));
	IceDecalComp->SetupAttachment(RootComponent);
	
	IceDecalComp->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	
	IceDecalComp->DecalSize = FVector(200.f, 350.f, 350.f); // X 가 두께임
	

}


void AIcePad::BeginPlay()
{
	Super::BeginPlay();
	
	// 서버에서만 충돌 로직 처리 (데미지나 CC기는 서버 권한이 정석임)
	if (HasAuthority())
	{
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AIcePad::OnOverlapBegin);
		CollisionBox->OnComponentEndOverlap.AddDynamic(this, &AIcePad::OnOverlapEnd);
		
		GetWorldTimerManager().SetTimer(DoTTimerHandle, this, &AIcePad::ApplyDoTDamage, 1.0f, true);
		GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &AIcePad::DeactivateAndDestroy, Duration, false);
	}
	
}

void AIcePad::DeactivateAndDestroy()
{
	if (!HasAuthority()) return;

	// ====================================================================
	// 1. 충돌 판정을 강제로 끕니다.
	// 언리얼 엔진은 충돌이 꺼지는 순간, 안에 있던 모든 물체에 대해 
	// 'OnOverlapEnd' 이벤트를 강제로 발생시킵니다!
	// (즉, 우리가 이미 OnOverlapEnd에 짜둔 완벽한 코팅 벗기기 로직이 알아서 다 돌아갑니다)
	// ====================================================================
	if (CollisionBox)
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 2. 바닥 그림도 안 보이게끔 모두에게 방송!
	Multicast_HidePad();

	// 3. 통신이 도달할 시간을 충분히 주고 자연사시킵니다. (0.2초 -> 1.0초로 넉넉하게!)
	SetLifeSpan(1.0f);
}

void AIcePad::Multicast_HidePad_Implementation()
{
	if (IceDecalComp)
	{
		IceDecalComp->SetVisibility(false);
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
			
			// 나 혼자 씌우지 말고 모두에게 방송!
			Multicast_SetOverlayMaterial(Player, FrostOverlayMaterial);
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
			
			//  나 혼자 씌우지 말고 모두에게 방송!
			Multicast_SetOverlayMaterial(Enemy, FrostOverlayMaterial);
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
			
			// 나갈 때 묻어있던 얼음 코팅 벗기라고 방송! (nullptr 전달)
			Multicast_SetOverlayMaterial(Player, nullptr);
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
			
			// 나갈 때 얼음 코팅 벗기라고 방송!
			Multicast_SetOverlayMaterial(Enemy, nullptr);
		}
	}
}

// 3. 장판 사라질 때 (뒷정리)
void AIcePad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 안전장치: DeactivateAndDestroy가 안 불리고 맵에서 튕겼을 때 서버의 속도만 롤백해줍니다.
	for (ABaseCharacter* Player : AffectedPlayers)
	{
		if (IsValid(Player)) Player->ApplySlowDebuff(false, SlowPercentage);
	}
	for (auto& Elem : EnemyOriginalSpeeds)
	{
		if (IsValid(Elem.Key) && Elem.Key->GetCharacterMovement()) 
			Elem.Key->GetCharacterMovement()->MaxWalkSpeed = Elem.Value;
	}

	Super::EndPlay(EndPlayReason);
}

void AIcePad::ApplyDoTDamage()
{
	if (!HasAuthority()) return;

	// 몬스터 장부(EnemyOriginalSpeeds)에 기록된 적들에게만 데미지를 줍니다.
	// (아군은 슬로우만 걸리고 도트 딜은 안 아프게 예외 처리됨!)
	for (auto& Elem : EnemyOriginalSpeeds)
	{
		ACharacter* Enemy = Elem.Key;
        
		// 살아있고 유효한 몬스터인지 확인 후 데미지 부여
		if (IsValid(Enemy))
		{
			UGameplayStatics::ApplyDamage(Enemy, DoTDamage, GetInstigatorController(), this, UDamageType::StaticClass());
		}
	}
}

// 모든 클라이언트 화면에서 실제로 텍스처를 씌우거나 벗기는 작업을 수행
void AIcePad::Multicast_SetOverlayMaterial_Implementation(ACharacter* TargetCharacter, UMaterialInterface* OverlayMat)
{
	if (TargetCharacter && TargetCharacter->GetMesh())
	{
		TargetCharacter->GetMesh()->SetOverlayMaterial(OverlayMat);
	}
}