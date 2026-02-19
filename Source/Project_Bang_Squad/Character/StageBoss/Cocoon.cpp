// Source/Project_Bang_Squad/Gimmick/Cocoon.cpp
#include "Cocoon.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h" // [필수] 캡슐 컴포넌트 헤더 추가
#include "Particles/ParticleSystem.h" // 파티클 시스템 헤더
#include "Sound/SoundBase.h"


ACocoon::ACocoon()
{
    // [해결포인트 1] 서버의 고치를 클라이언트에 똑같이 복제(Replicate)합니다.
    bReplicates = true;

    // [해결포인트 2] 위치(플레이어에게 붙어다니는 것)도 복제합니다.
    SetReplicateMovement(true);

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
}

void ACocoon::InitializeCocoon(ACharacter* TargetPlayer)
{
    TrappedPlayer = TargetPlayer;
    if (TrappedPlayer)
    {
        // 1. 일단 플레이어 중심에 딱 붙입니다.
        AttachToActor(TrappedPlayer, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

        // 2. [추가된 부분] 위치 보정 (플레이어 캡슐 높이의 절반만큼 아래로 내림)
        if (UCapsuleComponent* Capsule = TrappedPlayer->GetCapsuleComponent())
        {
            // 캡슐의 절반 높이(HalfHeight)의 0.6배 정도만큼 아래(-Z)로 내림
            // (이 0.6f 숫자를 조절해서 높낮이를 미세조정 하세요. 0.5~0.8 추천)
            float DownOffset = Capsule->GetScaledCapsuleHalfHeight() * 0.6f;

            // 현재 위치에서 Z축으로만 내립니다.
            AddActorLocalOffset(FVector(0.0f, 0.0f, -DownOffset));
        }

        // 3. 플레이어 조작 불가 처리 (기존 코드)
        if (APlayerController* PC = Cast<APlayerController>(TrappedPlayer->GetController()))
        {
            TrappedPlayer->DisableInput(PC);
        }
        TrappedPlayer->GetCharacterMovement()->SetMovementMode(MOVE_None);

        // 4. 타이머 시작 (기존 코드)
        GetWorld()->GetTimerManager().SetTimer(ExplosionTimerHandle, this, &ACocoon::Explode, 7.0f, false);
    }
}

float ACocoon::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    // 갇힌 본인이 때리는 건 무시 (혹시나 해서 추가)
    if (DamageCauser == TrappedPlayer) return 0.0f;

    // 아군이 때리면 카운트 증가
    HitCount++;
    if (HitCount >= 2)
    {
        ReleasePlayer(); // 2대 맞으면 구출
    }
    return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ACocoon::ReleasePlayer()
{
    // [권한 분리] 파괴는 서버가 전담합니다. 서버가 Destroy()하면 클라이언트의 복제본도 알아서 소멸합니다!
    if (!HasAuthority()) return;

    if (TrappedPlayer)
    {
        if (APlayerController* PC = Cast<APlayerController>(TrappedPlayer->GetController()))
        {
            TrappedPlayer->EnableInput(PC);
        }
        TrappedPlayer->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }

    GetWorld()->GetTimerManager().ClearTimer(ExplosionTimerHandle);
    Destroy();
}

void ACocoon::Explode()
{
    // [권한 분리] 폭발 데미지 연산도 서버만 담당합니다.
    if (!HasAuthority()) return;

    if (TrappedPlayer)
    {
        float MaxHP = 1000.0f; // 나중에 GetMaxHealth() 등으로 교체
        float Damage = MaxHP * 0.7f;
        UGameplayStatics::ApplyDamage(TrappedPlayer, Damage, nullptr, this, UDamageType::StaticClass());
    }

    // [비주얼 동기화] 데미지를 주었으니 모든 클라이언트에게 펑! 터지라고 방송합니다.
    Multicast_PlayExplosionVFX();

    ReleasePlayer(); // 폭발 후 풀어주기 (여기서 Destroy가 호출됨)
}

// [추가] 멀티캐스트 구현부: 모든 클라이언트 화면에서 이펙트 재생
void ACocoon::Multicast_PlayExplosionVFX_Implementation()
{
    if (ExplosionVFX)
    {
        FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 50.0f);
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, SpawnLoc, FRotator::ZeroRotator, FVector(3.0f));
    }

    if (ExplosionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
    }
}