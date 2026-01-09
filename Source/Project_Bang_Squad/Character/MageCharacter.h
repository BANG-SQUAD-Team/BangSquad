#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h"
#include "Components/TimelineComponent.h" // [필수] 타임라인 기능을 위해 추가
#include "MageCharacter.generated.h"

class UDataTable;
class APillar; 
class UCurveFloat; // [추가] 커브 클래스 전방 선언

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

    // =========================================================
    // [추가됨] 타임라인 & 커브 관련 변수
    // =========================================================
public: 
    // 타임라인 컴포넌트 (생성자에서 CreateDefaultSubobject 해야 함)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Timeline")
    UTimelineComponent* CameraTimelineComp;

    // 에디터에서 만든 CurveFloat 에셋을 넣을 변수
    UPROPERTY(EditAnywhere, Category = "Timeline")
    UCurveFloat* CameraCurve;

private:
    // =========================================================
    //  타임라인 내부 로직
    // =========================================================
    
    // 타임라인이 매 프레임 호출할 함수 (반드시 UFUNCTION 필요)
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    // 타임라인에 함수를 연결할 델리게이트 핸들
    FOnTimelineFloat UpdateCameraFloat;
    
    // 타임라인이 완전히 끝났을 때 호출될 함수
    UFUNCTION()
    void OnCameraTimelineFinished();


    // =========================================================
    // 기존 변수들
    // =========================================================
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
    
    // (타임라인을 쓰면 이 타이머는 이제 안 쓸 수도 있지만, 혹시 몰라 남겨둠)
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