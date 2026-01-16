#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "MagicInteractableInterface.generated.h"


UINTERFACE(MinimalAPI)
class UMagicInteractableInterface : public UInterface
{
	GENERATED_BODY()
};



class PROJECT_BANG_SQUAD_API IMagicInteractableInterface
{
	GENERATED_BODY()
public:

	// 기능 1 메이지가 쳐다볼 때 아웃라인 (Highlight) 켜기/끄기
	virtual void SetMageHighlight(bool bActive) = 0;
	// 기능 2 메이지가 염동력(우클릭)을 썼을 때 반응하기
	// Direction: 마우스로 조종한 방향
	virtual void ProcessMageInput(FVector Direction) = 0;
};
