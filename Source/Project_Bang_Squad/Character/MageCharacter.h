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
    
    // MageCharacter.h

public:
    // 서버한테 투사체 좀 쏴달라고 요청하는 함수
    UFUNCTION(Server, Reliable)
    void Server_SpawnProjectile(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation);
    void Server_SpawnProjectile_Implementation(UClass* ProjectileClassToSpawn, FVector Location, FRotator Rotation);

private:
    UFUNCTION()
    void CameraTimelineProgress(float Alpha);

    UFUNCTION()
    void OnCameraTimelineFinished();

    // =========================================================
    // [추가됨] 멀티플레이용 서버 RPC 함수
    // =========================================================
protected:
    // 클라이언트가 "나 기둥 밀었어!"라고 서버에 보내는 신호
    // WithValidation은 생략하고 Reliable(중요함)만 사용
    UFUNCTION(Server, Reliable)
    void Server_TriggerPillarFall(APillar* TargetPillar);

    // =========================================================
    // 일반 변수
    // =========================================================
private:
    float DefaultArmLength;
    FVector DefaultSocketOffset;
    
    void ProcessSkill(FName SkillRowName);
    
    void UpdatePillarInteraction();
    void LockOnPillar(float DeltaTime);
    
    UPROPERTY(EditAnywhere, Category = "Telekinesis")
    float TraceDistance = 5000.f;

    UPROPERTY()
    APillar* FocusedPillar;

    UPROPERTY()
    APillar* CurrentTargetPillar;

    bool bIsJobAbilityActive = false;
};