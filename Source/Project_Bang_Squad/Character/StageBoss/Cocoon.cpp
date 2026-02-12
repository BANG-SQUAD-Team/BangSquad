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
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;

    // 고치는 아군의 공격을 막아줘야(맞아야) 하므로 BlockAll
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
    // 1. 이펙트 재생 (크기를 좀 더 키우고, 위치를 살짝 위로)
    if (ExplosionVFX)
    {
        FVector SpawnLoc = GetActorLocation() + FVector(0, 0, 50.0f); // 고치 중심보다 살짝 위

        // 이펙트 스폰 (WorldContextObject, ParticleSystem, Location, Rotation, Scale, bAutoDestroy)
        UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionVFX, SpawnLoc, FRotator::ZeroRotator, FVector(3.0f)); // 크기 3배로 키움!
    }

    if (TrappedPlayer)
    {
        // 최대 체력의 70% 트루 데미지
        float MaxHP = 1000.0f; // 나중에 GetMaxHealth() 등으로 교체
        float Damage = MaxHP * 0.7f;

        // 이제 include가 있어서 에러가 나지 않습니다.
        UGameplayStatics::ApplyDamage(TrappedPlayer, Damage, nullptr, this, UDamageType::StaticClass());

        // 폭발 이펙트 추가 가능
        // UGameplayStatics::SpawnEmitterAtLocation(...);
    }

    ReleasePlayer(); // 폭발 후에도 풀어줌
}