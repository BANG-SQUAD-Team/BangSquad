#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DataTable.h"

AMageCharacter::AMageCharacter()
{
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	JumpCooldownTimer = 1.0f;   // 1초 쿨타임
	UnlockedStageLevel = 1; // 현재 진행 중인 스테이지 (테스트용)
	
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
	// 우클릭 직업 능력도 데이터 테이블에서 관리
	ProcessSkill(TEXT("JobAbility"));

}

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
	if (!SkillDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("Skill Data Missed."));
		return;
	}

	// 데이터테이블에서 행 찾기
	static const FString ContextString(TEXT("SkillContext"));
	FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

	if (Data)
	{
		// 1. 부모 클래스의 함수를 호출하여 스테이지 해금 여부 체크
		if (!IsSkillUnlocked(Data->RequiredStage))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
					FString::Printf(TEXT("You Can't Use Skill!(Required Stage:%d)"), Data->RequiredStage));
			}
			return;
		}

		// 2. 몽타주가 있다면 재생
		if (Data->SkillMontage)
		{
			PlayAnimMontage(Data->SkillMontage);
		}
		// 3. 기능 실행
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("%s ! (Damage: %.1f)"), *Data->SkillName.ToString(), Data->Damage));
		}

		// TODO 실제 투사체 생성 로직이 여기 들어가야함

	}

	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't Found [%s]"), *SkillRowName.ToString());
	}
}
