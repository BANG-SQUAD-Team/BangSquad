// Source/Project_Bang_Squad/AnimNotify/AnimNotify_Knockback.cpp

#include "AnimNotify_Knockback.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"

// [추가] 플레이어인지 확실히 구분하기 위해 헤더 포함
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" 

UAnimNotify_Knockback::UAnimNotify_Knockback()
{
    NotifyColor = FColor(255, 100, 100, 255);
}

void UAnimNotify_Knockback::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetWorld()) return;

    AActor* OwnerActor = MeshComp->GetOwner();
    if (!OwnerActor) return;

    // 1. 서버 권한 확인
    if (!OwnerActor->HasAuthority()) return;

    // 2. 공격 범위 설정
    FVector Start = OwnerActor->GetActorLocation();
    FVector Forward = OwnerActor->GetActorForwardVector();
    FVector End = Start + (Forward * AttackRadius);

    // 3. 충돌 검사
    TArray<FHitResult> HitResults;
    FCollisionShape Shape = FCollisionShape::MakeSphere(AttackRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerActor);

    // Pawn(캐릭터) 채널만 감지
    bool bHit = MeshComp->GetWorld()->SweepMultiByChannel(
        HitResults,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        Shape,
        Params
    );

    if (bHit)
    {
        TSet<AActor*> HitVictims;

        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();

            if (HitActor && HitActor != OwnerActor && !HitVictims.Contains(HitActor))
            {
                HitVictims.Add(HitActor);

                // [핵심 수정] ACharacter 대신 ABaseCharacter(플레이어)인지 확인!
                ABaseCharacter* Player = Cast<ABaseCharacter>(HitActor);

                // 플레이어인 경우에만 로직 실행
                if (Player)
                {
                    // 1. 데미지 적용
                    UGameplayStatics::ApplyDamage(
                        HitActor,
                        DamageAmount,
                        OwnerActor->GetInstigatorController(),
                        OwnerActor,
                        UDamageType::StaticClass()
                    );

                    // 2. 넉백 적용
                    // 몬스터가 보는 방향(앞)으로 밀어버림
                    FVector PushDir = Forward;
                    FVector LaunchForce = (PushDir * KnockbackStrength) + FVector(0.f, 0.f, KnockbackUpForce);

                    Player->LaunchCharacter(LaunchForce, true, true);
                }
            }
        }
    }
}