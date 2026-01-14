#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 
#include "MageCharacter.generated.h"

// 클래스 전방 선언
class UDataTable;
class UPrimitiveComponent;

UCLASS()
class PROJECT_BANG_SQUAD_API AMageCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AMageCharacter();


	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	/** 스킬 기능 재정의 */
	virtual void Attack() override;
	virtual void Skill1() override;
	virtual void Skill2() override;
    
	// JobAbility(우클릭): 단발성 염력 타격(Impulse)
	virtual void JobAbility() override;

	// EndJobAbility: 우클릭 뗌 (힘 적용 중단)
	void EndJobAbility();

	/** 데이터 테이블 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
	UDataTable* SkillDataTable;

private:
	/** 내부 로직 함수 */
    
	// 데이터 테이블 기반 스킬 실행
	void ProcessSkill(FName SkillRowName);
	
	// 매 프레임 실행: 크로스헤어 아래 물체 확인 및 아웃라인 처리
	void UpdateCrosshairInteraction();

	// 염력 사거리 (기존 700 -> 1500으로 증가, 멀리서도 쓰러뜨리기 위해)
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	float TraceDistance = 1500.f;

	// 염력 밀기 강도 (새로 추가됨)
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	float PushForce = 5000000.f;
	
	// 현재 내가 우클릭으로 잡고(밀고)있는 물리 컴포넌트
	UPROPERTY()
	UPrimitiveComponent* CurrentTargetComp;
	
	// 현재 시선이 닿아 아웃라인(테두리)이 켜진 컴포넌트
	UPROPERTY()
	UPrimitiveComponent* CurrentFocusedComp;
	
};