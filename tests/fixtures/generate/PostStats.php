<?php
declare(strict_types=1);

namespace App\Admin\Widgets;

use App\Admin\Resources\PostResource;
use FloCMS\Admin\Widgets\Stat;
use FloCMS\Admin\Widgets\StatsWidget;
use FloCMS\Admin\Widgets\WidgetContext;

/**
 * Dashboard statistics: up to eight Stat cards.
 * Registered in config/admin.php under 'widgets'.
 */
final class PostStats extends StatsWidget
{
    protected string $title = 'Poststats'; // plain text or a site language key

    // Every resource read below; users without its view permission do not see the widget.
    protected array $resources = [PostResource::class];

    protected int|string $columnSpan = 'full'; // 1-4 or 'full'
    protected int $sort = 0;

    protected function stats(WidgetContext $context): array
    {
        $records = $context->query(PostResource::class);

        return [
            Stat::make('Total', $records->count())->resource(PostResource::class),
            // Stat::make('Published', $records->where('status', '=', 'published')->count())->color('success'),
        ];
    }
}
