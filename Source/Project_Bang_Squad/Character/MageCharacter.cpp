#include "Project_Bang_Squad/Character/MageCharacter.h"
#include "Project_Bang_Squad/Projectile/MageProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h" 
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DataTable.h"
#include "Components/PrimitiveComponent.h" 


AMageCharacter::AMageCharacter()
{
    // 매 프레임 아웃라인 체크와 힘 적용을 위해 Tick 활성화
    PrimaryActorTick.bCanEverTick = true;

    GetCharacterMovement()->MaxWalkSpeed = 500.f;
    JumpCooldownTimer = 1.0f;
    UnlockedStageLevel = 1;

    // 포인터 초기화
    CurrentTargetComp = nullptr;
    CurrentFocusedComp = nullptr;

    // [삭제됨] PhysicsHandle, HoldPoint 생성 코드 제거
}

void AMageCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (JobAbilityAction)
        {
            // 누름(타겟 지정)과 뗌(힘 중단) 모두 바인딩
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Started, this, &AMageCharacter::JobAbility);
            EIC->BindAction(JobAbilityAction, ETriggerEvent::Completed, this, &AMageCharacter::EndJobAbility);
        }
    }
}

void AMageCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1. 평상시: 크로스헤어 아래 물체 아웃라인(테두리) 처리
    UpdateCrosshairInteraction();

    // 2. 스킬 사용 중: 타겟을 잡고 있다면, 내 시선 방향으로 힘을 가함
    if (CurrentTargetComp && Camera)
    {
        // A. 카메라가 보고 있는 목표 지점 계산 (기둥 뒤의 바닥이나 허공)
        FHitResult HitResult;
        FVector Start = Camera->GetComponentLocation();
        FVector End = Start + (Camera->GetForwardVector() * TraceDistance * 1.5f); // 넉넉한 거리

        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);
        Params.AddIgnoredComponent(CurrentTargetComp); // 내가 밀고 있는 기둥은 무시해야 그 뒤를 볼 수 있음

        FVector TargetLocation = End; // 허공을 보면 끝점이 목표

        // 레이저가 무언가(바닥 등)에 닿았다면 그곳이 목표 지점
        if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
        {
            TargetLocation = HitResult.Location;
        }

        // B. 힘의 방향 계산: [기둥의 윗부분] -> [목표 지점]
        // 기둥의 중심(Origin)에서 Z축으로 반(BoxExtent.Z)만큼 올리면 윗부분입니다.
        FVector PillarTop = CurrentTargetComp->Bounds.Origin + FVector(0, 0, CurrentTargetComp->Bounds.BoxExtent.Z);
        FVector ForceDir = (TargetLocation - PillarTop).GetSafeNormal();

        // C. 지속적인 힘(Force) 가하기
        // AddForce는 질량에 영향을 받으므로 PushForce 값이 매우 커야 합니다.
        CurrentTargetComp->AddForceAtLocation(ForceDir * PushForce, PillarTop);
    }
}

void AMageCharacter::UpdateCrosshairInteraction()
{
    if (!Camera) return;

    FHitResult HitResult;
    FVector Start = Camera->GetComponentLocation();
    FVector End = Start + (Camera->GetForwardVector() * TraceDistance);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // 1. 레이저 발사
    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
    UPrimitiveComponent* HitComp = bHit ? HitResult.GetComponent() : nullptr;

    // 2. 물리 시뮬레이션 중인 물체인지 확인
    bool bIsPhysicsObject = (HitComp && HitComp->IsSimulatingPhysics());

    // 3. 상태 변화 체크 (새로운 물체를 봤거나, 허공을 봤을 때)
    if (CurrentFocusedComp != HitComp)
    {
        // 기존에 보고 있던 게 있다면 아웃라인 끄기
        if (CurrentFocusedComp)
        {
            CurrentFocusedComp->SetRenderCustomDepth(false);
        }

        // 새로 본 게 물리 오브젝트라면 아웃라인 켜기
        if (bIsPhysicsObject)
        {
            HitComp->SetRenderCustomDepth(true);
            // (선택) 아웃라인 색상 설정 로직이 있다면 여기서 CustomDepthStencilValue 설정
            CurrentFocusedComp = HitComp;
        }
        else
        {
            CurrentFocusedComp = nullptr;
        }
    }
}

void AMageCharacter::JobAbility()
{
    // 1. 애니메이션/사운드 재생 (데이터 테이블)
    ProcessSkill(TEXT("JobAbility"));

    // 2. 현재 아웃라인이 켜져 있는(보고 있는) 물체가 있다면 타겟으로 설정
    if (CurrentFocusedComp)
    {
        CurrentTargetComp = CurrentFocusedComp;

        if (GEngine) 
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("염력 제어 시작! 마우스로 방향을 지시하세요."));
    }
}

void AMageCharacter::EndJobAbility()
{
    // 버튼을 떼면 타겟 해제 -> Tick에서 힘 주는 것을 멈춤
    if (CurrentTargetComp)
    {
        CurrentTargetComp = nullptr;
        if (GEngine) 
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Silver, TEXT("염력 해제"));
    }
}

void AMageCharacter::ProcessSkill(FName SkillRowName)
{
    // 기존 로직 유지
    if (!SkillDataTable) return;

    static const FString ContextString(TEXT("SkillContext"));
    FSkillData* Data = SkillDataTable->FindRow<FSkillData>(SkillRowName, ContextString);

    if (Data)
    {
       if (!IsSkillUnlocked(Data->RequiredStage)) return;

       if (Data->SkillMontage)
       {
          PlayAnimMontage(Data->SkillMontage);
       }

        if (Data->ProjectileClass)
        {
           FVector SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.0f;
           FRotator SpawnRotation = GetControlRotation();

           FActorSpawnParameters SpawnParams;
           SpawnParams.Owner = this;
           SpawnParams.Instigator = GetInstigator();

           AMageProjectile* Projectile = GetWorld()->SpawnActor<AMageProjectile>(
             Data->ProjectileClass, 
             SpawnLocation, 
             SpawnRotation, 
             SpawnParams
          );

           if (Projectile)
           {
              Projectile->Damage = Data->Damage;
           }
        }
    }
    else
    {
        // 데이터 테이블에 행이 없어도 염력 로직은 작동하므로 크래시 방지용 로그만 출력
        // UE_LOG(LogTemp, Warning, TEXT("스킬 데이터를 찾을 수 없습니다: %s"), *SkillRowName.ToString());
    }
}

// Attack, Skill1, Skill2는 그대로 유지
void AMageCharacter::Attack() { ProcessSkill(TEXT("Attack")); }
void AMageCharacter::Skill1() { ProcessSkill(TEXT("Skill1")); }
void AMageCharacter::Skill2() { ProcessSkill(TEXT("Skill2")); }