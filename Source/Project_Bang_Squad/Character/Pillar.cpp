// Fill out your copyright notice in the Description page of Project Settings.


#include "Project_Bang_Squad/Character/Pillar.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

APillar::APillar()
{
	PrimaryActorTick.bCanEverTick = false;

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	RootComponent = PillarMesh;

	// 물리 및 충돌 설정
	PillarMesh->SetSimulatePhysics(false); // 처음엔 고정
	PillarMesh->SetNotifyRigidBodyCollision(true); // Hit 이벤트 활성화
}

void APillar::BeginPlay()
{
	Super::BeginPlay();
	PillarMesh->OnComponentHit.AddDynamic(this, &APillar::OnPillarHit);
}

void APillar::TriggerFall()
{
	if (bIsFallen || !PillarMesh) return;

	bIsFallen = true;
    
	// 1. 물리 강제 활성화 및 잠듬 깨우기
	PillarMesh->SetSimulatePhysics(true);
	PillarMesh->WakeAllRigidBodies();

	// 2. 캐릭터 충돌을 Overlap으로 변경 (기획 5번: 캐릭터에 막히지 않음)
	PillarMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 3. 넘어지는 방향 벡터 (FallDirection)
	FVector LaunchDir = FallDirection.GetSafeNormal();

	// [보강] 각속도(회전하는 힘)를 직접 부여하여 즉시 회전하게 만듦
	// 기둥 옆면을 쳐서 돌리는 느낌의 축 계산
	FVector TorqueAxis = FVector::CrossProduct(FVector::UpVector, LaunchDir);
	PillarMesh->SetPhysicsAngularVelocityInDegrees(TorqueAxis * 100.f);

	// 4. 강력한 충격 가하기 (기둥 윗부분 타격)
	FVector ApplyLocation = GetActorLocation() + FVector(0, 0, 400.f);
	PillarMesh->AddImpulseAtLocation(LaunchDir * FallForce, ApplyLocation);

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("기둥 작동 완료!"));
}

void APillar::OnPillarHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 바닥이나 월드 정적 물체에 닿으면 데미지 발생
	if (bIsFallen && OtherComp->GetCollisionObjectType() == ECC_WorldStatic)
	{
		// 주변 적에게 데미지 + 밀쳐내기 (Radial Damage)
		UGameplayStatics::ApplyRadialDamage(GetWorld(), 50.f, GetActorLocation(), 500.f, nullptr, {}, this);
        
		// 시각 효과나 카메라 흔들림 추가 가능
        
		// 3초 뒤 기둥 삭제
		SetLifeSpan(3.0f);
	}
}