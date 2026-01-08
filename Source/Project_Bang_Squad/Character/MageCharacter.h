#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "MageCharacter.generated.h"

class UDataTable;
class APillar; // Pillar 클래스 전방 선언

UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AMageCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;
	
	/** 스킬 기능 재정의 */
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;
    
	// JobAbility(우클릭): 타겟 고정 및 조작 시작
	virtual void JobAbility() override;
	void EndJobAbility();

	/** 데이터 테이블 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	UDataTable* SkillDataTable;

private:
	// 에디터에서 설정한 기본 팔 길이와 오프셋을 저장할 변수
	float DefaultArmLength;
	FVector DefaultSocketOffset;
	
	/** 내부 로직 함수 */
	void ProcessSkill(FName SkillRowName);
    
	// 매 프레임 실행: 크로스헤어 아래 Pillar 확인 및 아웃라인 처리
	void UpdatePillarInteraction();

	// 시선 고정 로직
	void LockOnPillar(float DeltaTime);
	
	// 카메라 복귀 지연을 위한 타이머 핸들
	FTimerHandle CameraResetTimerHandle;

	// 염력 사거리
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	float TraceDistance = 5000.f;

	// 현재 메이지가 조준/조작 중인 기둥 액터
	UPROPERTY()
	APillar* FocusedPillar;

	UPROPERTY()
	APillar* CurrentTargetPillar;

	// 조작 중인지 여부
	bool bIsJobAbilityActive = false;
};