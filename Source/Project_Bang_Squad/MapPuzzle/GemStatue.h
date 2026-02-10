#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GemStatue.generated.h"

class ACenterStatueManager;

// 보석 액터를 전방 선언 (실제 구현은 그냥 AActor 상속받고 피격 시 Destroy 호출하면 됨)
class AActor;

UCLASS()
class PROJECT_BANG_SQUAD_API AGemStatue : public AActor
{
	GENERATED_BODY()

public:
	AGemStatue();

protected:
	virtual void BeginPlay() override;

public:
	// =================================================================
	// 컴포넌트
	// =================================================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StatueMesh;

	// 활성화되면 나타날 추가 메쉬 (예: 석상이 들고 있는 무언가)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* HiddenAddonMesh;

	// =================================================================
	// 설정
	// =================================================================
	// [필수] 중앙 석상 연결
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	ACenterStatueManager* CenterStatue;

	// [필수] 감시할 보석들 (에디터에서 스포이드로 찍어줌)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Link")
	TArray<AActor*> TargetGems;

	// =================================================================
	// 함수
	// =================================================================
	// 보석 하나가 파괴될 때마다 호출
	UFUNCTION()
	void CheckGemStatus(AActor* DestroyedGem);

private:
	bool bIsActivated = false;
	int32 CurrentGemCount = 0;
};