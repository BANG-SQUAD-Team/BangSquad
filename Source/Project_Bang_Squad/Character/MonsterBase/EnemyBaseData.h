// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyBaseData.generated.h"

/**
 * [Base Class]
 * 모든 몬스터(일반, 보스)가 공통으로 가져야 할 데이터를 정의합니다.
 * UPrimaryDataAsset을 상속받으면 에셋 관리(Asset Manager)에서 검색하기 더 유리합니다.
 */
UCLASS(BlueprintType, Abstract) // Abstract: 이 클래스 자체로는 에셋 생성을 막음 (자식을 써라)
class PROJECT_BANG_SQUAD_API UEnemyBaseData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- [Visuals] 외형 정보 ---

	// 몬스터의 스켈레탈 메시 (Visual)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Visual")
	TObjectPtr<USkeletalMesh> Mesh;

	// 사용할 애니메이션 블루프린트 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	// --- [Stats] 기본 스탯 ---

	// 최대 체력 (기본값 100)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	// 이동 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Stats")
	float WalkSpeed = 300.0f;

	// 공격 범위
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base|Stats")
	float AttackRange = 150.0f;
};