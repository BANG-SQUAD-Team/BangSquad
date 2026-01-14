#include "Project_Bang_Squad/Character/StrikerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"

AStrikerCharacter::AStrikerCharacter()
{
	GetCharacterMovement()->MaxWalkSpeed = 700.f; // 스트라이커는 빠르게
}

void AStrikerCharacter::JobAbility()
{
	AActor* Target = FindAirborneTarget();
	if (Target)
	{
		// 1. 적 근처로 순간이동
		FVector DashLocation = Target->GetActorLocation() - Target->GetActorForwardVector() * 50.f;
		SetActorLocation(DashLocation);

		// 2. 공격 몽타주 재생 및 데미지 (ProcessSkill 활용 가능)
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Airborne Slash!"));
	}
}

AActor* AStrikerCharacter::FindAirborneTarget()
{
	TArray<AActor*> OverlappingActors;
	// 캐릭터 주변 1000 유닛 범위 탐색
	UKismetSystemLibrary::SphereOverlapActors(GetWorld(), GetActorLocation(), 1000.f,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn) }, ACharacter::StaticClass(), { this }, OverlappingActors);

	for (AActor* Actor : OverlappingActors)
	{
		if (ACharacter* Enemy = Cast<ACharacter>(Actor))
		{
			// 적이 공중에 떠 있는지 확인 (IsFalling)
			if (Enemy->GetCharacterMovement()->IsFalling())
			{
				return Enemy;
			}
		}
	}
	return nullptr;
}

void AStrikerCharacter::Skill1()
{
	ProcessSkill(TEXT("Skill1"));
	// 내려치기 성공 시 타격 대상에게 LaunchCharacter로 띄우기 적용 필요
}

void AStrikerCharacter::Skill2()
{
	if (GetCharacterMovement()->IsFalling())
	{
		ProcessSkill(TEXT("Skill2")); // 공중 찍기
	}
}

void AStrikerCharacter::ProcessSkill(FName SkillRowName)
{
	// 데이터 테이블 기반 공통 로직
}