#include "Project_Bang_Squad/Character/MonsterBase/EnemyCharacterBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Project_Bang_Squad/Character/Component/HealthComponent.h"
#include "Project_Bang_Squad/Core/BSGameInstance.h"
#include "Project_Bang_Squad/Projectile/SlashProjectile.h"
#include "Project_Bang_Squad/UI/Enemy/EnemyNormalHPWidget.h"
#include "Project_Bang_Squad/Character/Damage/DamageTextActor.h"

AEnemyCharacterBase::AEnemyCharacterBase()
{
    PrimaryActorTick.bCanEverTick = false;
    bUseControllerRotationYaw = false;

    bReplicates = true;
	
	HealthWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComp"));
	HealthWidgetComp->SetupAttachment(GetRootComponent());
	HealthWidgetComp->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f)); // 머리 위쪽
	HealthWidgetComp->SetWidgetSpace(EWidgetSpace::Screen); // 화면에 딱 붙게
	HealthWidgetComp->SetDrawSize(FVector2D(100.0f, 15.0f));
	HealthWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 충돌 무시
}

void AEnemyCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	GenerateUniqueID();

	UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance());
	if (GI && GI->IsMonsterDead(MyUniqueID))
	{
		Destroy();
		return; 
	}
    
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = Move->MaxWalkSpeed;
		Move->bUseControllerDesiredRotation = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}

	// =========================
	// HealthComponent 자동 바인딩 및 초기화
	// =========================
	if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
	{
		//  게임 시작 시 현재 체력을 최대 체력과 똑같이 맞춤
		if (HasAuthority())
		{
			HC->CurrentHealth = HC->MaxHealth;
		}

		// 기존 델리게이트 연결
		HC->OnDead.RemoveDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);
		HC->OnDead.AddDynamic(this, &AEnemyCharacterBase::HandleDeadFromHealth);

		HC->OnHealthChanged.RemoveDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
		HC->OnHealthChanged.AddDynamic(this, &AEnemyCharacterBase::HandleHealthChangedFromHealth);
		UpdateHealthBar(HC->MaxHealth, HC->MaxHealth);
	}
}

float AEnemyCharacterBase::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
    class AController* EventInstigator, AActor* DamageCauser)
{
    // 1. 서버 권한 확인 (데미지 처리는 서버에서만)
    if (!HasAuthority()) return 0.0f;

    // ==============================================================
    // [팀킬 방지 로직 추가]
    // 공격자(Instigator의 Pawn 혹은 DamageCauser의 Owner)를 찾습니다.
    // ==============================================================
    AActor* Attacker = DamageCauser;

    // Instigator(가해자의 컨트롤러)가 있다면 Pawn을 가져옵니다. (가장 정확함)
    if (EventInstigator && EventInstigator->GetPawn())
    {
        Attacker = EventInstigator->GetPawn();
    }
    // 투사체인 경우, 투사체의 주인을 가져옵니다.
    else if (DamageCauser && DamageCauser->GetOwner())
    {
        Attacker = DamageCauser->GetOwner();
    }

    // 공격자가 존재할 때 팀킬 여부 확인
    if (Attacker)
    {
        // 1. 자기 자신이 낸 데미지 무시 (예: 내 칼에 내가 맞음 방지)
        if (Attacker == this) return 0.0f;

        // 2. 같은 몬스터 팀인지 확인
        // AEnemyCharacterBase를 상속받은 객체(=몬스터)라면 데미지 0 처리
        if (Attacker->IsA(AEnemyCharacterBase::StaticClass()))
        {
            // (디버깅용 로그 - 필요시 주석 해제)
            // UE_LOG(LogTemp, Warning, TEXT("Friendly Fire Blocked! Attacker: %s -> Victim: %s"), *Attacker->GetName(), *GetName());
            return 0.0f;
        }
    }
    // ==============================================================

    // 2. 부모 클래스의 기본 로직 실행 (Super 호출)
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    // 3. HealthComponent를 찾아서 데미지 적용
    if (UHealthComponent* HC = FindComponentByClass<UHealthComponent>())
    {
        // 이미 죽었으면 무시
        if (HC->IsDead()) return 0.0f;

        // 여기서 HealthComponent의 체력이 깎이고 -> OnHealthChanged 델리게이트가 호출됨
        HC->ApplyDamage(ActualDamage);
    }
	
	// =================================================================
	// 실제 데미지가 0보다 크면, 클라이언트들에게 숫자 띄우기 지시!
	// =================================================================
	if (ActualDamage > 0.0f)
	{
		Multicast_SpawnDamageText(ActualDamage, GetActorLocation());
	}

    return ActualDamage;
}
void AEnemyCharacterBase::OnDeathStarted()
{
}

void AEnemyCharacterBase::HandleDeadFromHealth()
{
    ReceiveDeath();
}


// 빈 가상 함수 구현 (자식이 없으면 아무 일도 안 함)
void AEnemyCharacterBase::OnHPChanged(float CurrentHP, float MaxHP)
{
	
}

void AEnemyCharacterBase::HandleHealthChangedFromHealth(float NewHealth, float InMaxHealth)
{
	
	OnHPChanged(NewHealth, InMaxHealth);
	
    if (HasAuthority())
    {
	    Multicast_UpdateHealthBar(NewHealth, InMaxHealth);
    }
	
	if (!HasAuthority()) return;
    if (bIsDead) return;
    if (NewHealth <= 0.f) return;

    // 데미지를 입었으니 피격 모션 재생
    ReceiveHitReact();
}

// =========================
// Hit React
// =========================

void AEnemyCharacterBase::ReceiveHitReact()
{
    if (!HasAuthority()) return;
    if (bIsDead) return;

    if (bIsHitReacting && bIgnoreHitReactWhileActive)
    {
       return;
    }

    float Duration = HitReactMinDuration;
    if (HitReactMontage)
    {
       Duration = FMath::Max(Duration, HitReactMontage->GetPlayLength());
    }

    StartHitReact(Duration);
}

void AEnemyCharacterBase::StartHitReact(float Duration)
{
    bIsHitReacting = true;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       if (DefaultMaxWalkSpeed <= 0.f)
       {
          DefaultMaxWalkSpeed = Move->MaxWalkSpeed;
       }

       Move->MaxWalkSpeed = DefaultMaxWalkSpeed * HitReactSpeedMultiplier;
    }

    Multicast_PlayHitReactMontage();

    GetWorldTimerManager().ClearTimer(HitReactTimer);
    GetWorldTimerManager().SetTimer(HitReactTimer, this, &AEnemyCharacterBase::EndHitReact, Duration, false);
}

void AEnemyCharacterBase::EndHitReact()
{
    bIsHitReacting = false;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       Move->MaxWalkSpeed = DefaultMaxWalkSpeed;
    }
}

void AEnemyCharacterBase::Multicast_PlayHitReactMontage_Implementation()
{
    if (!HitReactMontage) return;
    if (bIsDead) return;

    PlayAnimMontage(HitReactMontage);
}

// =========================
// Death
// =========================

void AEnemyCharacterBase::ReceiveDeath()
{
    if (!HasAuthority()) return;
    if (bIsDead) return;

    StartDeath();
}

void AEnemyCharacterBase::StartDeath()
{
    bIsDead = true;
    bIsHitReacting = false;

	if (HasAuthority())
	{
		UBSGameInstance* GI = Cast<UBSGameInstance>(GetGameInstance());
		if (GI)
		{
			GI->MarkMonsterAsDead(MyUniqueID);
		}
	}
	
    OnDeathStarted();

    if (AAIController* AIC = Cast<AAIController>(GetController()))
    {
       AIC->StopMovement();
       AIC->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
       Move->StopMovementImmediately();
       Move->DisableMovement();
    }

    Multicast_PlayDeathMontage();

    GetWorldTimerManager().ClearTimer(DeathToRagdollTimer);
    if (bEnableRagdollOnDeath)
    {
       GetWorldTimerManager().SetTimer(
          DeathToRagdollTimer,
          this,
          &AEnemyCharacterBase::EnterRagdoll,
          DeathToRagdollDelay,
          false
       );
    }

    if (bDestroyAfterDeath)
    {
       SetLifeSpan(DestroyDelay);
    }
}

void AEnemyCharacterBase::Multicast_PlayDeathMontage_Implementation()
{
    if (!DeathMontage) return;

    PlayAnimMontage(DeathMontage);

    if (bLoopDeathMontage && DeathLoopSectionName != NAME_None)
    {
       if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
       {
          AnimInst->Montage_SetNextSection(DeathLoopSectionName, DeathLoopSectionName, DeathMontage);
       }
    }
}

void AEnemyCharacterBase::EnterRagdoll()
{
    if (!HasAuthority()) return;
    Multicast_EnterRagdoll();
}

void AEnemyCharacterBase::Multicast_EnterRagdoll_Implementation()
{
    if (!GetMesh()) return;

    if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
    {
       AnimInst->StopAllMontages(0.1f);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
       Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    GetMesh()->WakeAllRigidBodies();
    GetMesh()->bBlendPhysics = true;
}

void AEnemyCharacterBase::GenerateUniqueID()
{
	FVector SpawnLocation = GetActorLocation();

	uint32 LocationHash = GetTypeHash(SpawnLocation);
	uint32 ClassHash = GetTypeHash(GetClass()->GetName());

	MyUniqueID = HashCombine(LocationHash, ClassHash);
}

void AEnemyCharacterBase::AnimNotify_SpawnSlash()
{
    // [원칙 1] 서버에서만 실행
    if (!HasAuthority() || !SlashClass) return;

    // 1. 기준점: 캡슐의 정중앙 (보통 허리 위치)
    FVector SpawnLocation = GetActorLocation();

    // 2. [기존 방식 복구] 앞방향으로 일정 거리 띄우기 (ForwardVector 활용)
    SpawnLocation += GetActorForwardVector() * SlashForwardOffset;

    // 3. [높이 조절] 블루프린트에서 설정한 값만큼 위아래로 이동
    SpawnLocation.Z += SlashZOffset;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    // 2. 스폰 후 데미지 전달
    ASlashProjectile* Projectile = GetWorld()->SpawnActor<ASlashProjectile>(
        SlashClass, SpawnLocation, GetActorRotation(), SpawnParams);

    if (Projectile)
    {
        // [핵심] 보스의 SlashDamage 변수값을 프로젝타일의 Damage 변수에 복사!
        Projectile->Damage = SlashDamage;
    }

}

void AEnemyCharacterBase::Multicast_SpawnDamageText_Implementation(float Damage, FVector Location)
{
	// 화면이 없는 데디케이티드 서버이거나, 블루프린트 설정이 안 되어있다면 무시
	if (IsRunningDedicatedServer() || !DamageTextClass) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 몬스터 머리 위쯤(Z축 + 120)에 데미지 액터 소환
	FVector SpawnLoc = Location + FVector(0.f, 0.f, 120.f);
    
	// 소환 실행
	ADamageTextActor* TextActor = GetWorld()->SpawnActor<ADamageTextActor>(
		DamageTextClass, 
		SpawnLoc, 
		FRotator::ZeroRotator, 
		Params
	);

	// 액터가 성공적으로 생겼다면 데미지 수치(숫자) 전달
	if (TextActor)
	{
		TextActor->InitializeDamage(Damage);
	}
}

void AEnemyCharacterBase::Multicast_UpdateHealthBar_Implementation(float CurrentHP, float InMaxHP)
{
	UpdateHealthBar(CurrentHP, InMaxHP);
}

void AEnemyCharacterBase::UpdateHealthBar(float CurrentHP, float MaxHP)
{
	if (HealthWidgetComp)
	{
		// 위젯 컴포넌트에서 실제 UUserWidget 객체 가져오기
		UEnemyNormalHPWidget* HPWidget = Cast<UEnemyNormalHPWidget>(HealthWidgetComp->GetUserWidgetObject());
        
		if (HPWidget)
		{
			// 위젯 갱신 (블루프린트의 UpdateHP 함수 호출)
			HPWidget->UpdateHP(CurrentHP, MaxHP);
		}
	}
}