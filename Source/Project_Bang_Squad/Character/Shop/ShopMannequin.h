// ShopMannequin.h
#pragma once

#include "CoreMinimal.h"
#include "Project_Bang_Squad/Character/Base/BaseCharacter.h" // 경로 확인
#include "ShopMannequin.generated.h"

UCLASS()
class PROJECT_BANG_SQUAD_API AShopMannequin : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AShopMannequin();

protected:
	virtual void BeginPlay() override;
};