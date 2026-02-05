
#include "Project_Bang_Squad/UI/Enemy/EnemyNormalHPWidget.h"
#include "Components/ProgressBar.h"

void UEnemyNormalHPWidget::UpdateHP(float CurrentHP, float MaxHP)
{

	if (HPProgressBar && MaxHP > 0.0f)
	{
		// 0.0~1.0 비율 계산
		float Percent = FMath::Clamp(CurrentHP / MaxHP, 0.0f, 1.0f);
		HPProgressBar->SetPercent(Percent);
	}
}
