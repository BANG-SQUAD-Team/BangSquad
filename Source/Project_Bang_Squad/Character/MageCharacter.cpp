#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"

AMageCharacter::AMageCharacter()
{
	// 마법사 특화 설정
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
}

void AMageCharacter::Skill1()
{
	ProcessSkill(TEXT("Skill1"));
}

void AMageCharacter::Skill2()
{
	ProcessSkill(TEXT("Skill2"));
}

void AMageCharacter::JobAbility()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple, TEXT("마법사 직업능력 사용"));
	}

}

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 데이터가 없습니다."));
		return;
	}

	// 현재는 로그만 출력, 나중에 이쪽에 스킬 및 데미지 투사체 처리 하면 됨.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
			FString::Printf(TEXT("마법사 스킬 실행 : %s"), *SkillRowName.ToString()));
	}
}