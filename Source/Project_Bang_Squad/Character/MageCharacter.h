#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/TimelineComponent.h" 
#include "MageCharacter.generated.h"

class UDataTable;
class APillar; 
class UCurveFloat; 

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
    
    virtual void Attack() override;
    virtual void Skill1() override;
    virtual void Skill2() override;
    
    // 우클릭(JobAbility): 타겟 고정 및 조작 시작
    virtual void JobAbility() override;
    void EndJobAbility();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Data")
    UDataTable* SkillDataTable;

    // =========================================================
    // 타임라인 & 커브
    // =========================================================
public: 
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timeline")
    UTimelineComponent* CameraTimelineComp;

    UPROPERTY(EditAnywhere, Category = "Timeline")
    UCurveFloat* CameraCurve;

private:
    // 타임라인 진행 (매 프레임 호출)
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    // 타임라인 종료 (완료 시 호출)
    UFUNCTION()
    void OnCameraTimelineFinished();

    // =========================================================
    // 일반 변수
    // =========================================================
private:
    float DefaultArmLength;
    FVector DefaultSocketOffset;
    
    void ProcessSkill(FName SkillRowName);
    
    // 아웃라인 업데이트
    void UpdatePillarInteraction();

    // 시선 고정 로직
    void LockOnPillar(float DeltaTime);
    
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    UPROPERTY()
    APillar* FocusedPillar;

    UPROPERTY()
    APillar* CurrentTargetPillar;

    bool bIsJobAbilityActive = false;
};