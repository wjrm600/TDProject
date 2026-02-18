#include "AOSHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UAOSHealthBarWidget::UpdateHealthPercent(float Percent)
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}

void UAOSHealthBarWidget::SetBarColor(FLinearColor Color)
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetFillColorAndOpacity(Color);
	}
}
