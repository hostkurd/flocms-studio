<?php
declare(strict_types=1);

namespace App\Admin\Resources;

use FloCMS\Admin\Form\Fields\TextInput;
use FloCMS\Admin\Form\Form;
use FloCMS\Admin\Resource;
use FloCMS\Admin\Table\Columns\DateColumn;
use FloCMS\Admin\Table\Columns\TextColumn;
use FloCMS\Admin\Table\Table;

final class PropertyResource extends Resource
{
    protected static string $table = 'properties';

    // URL: /admin/listings
    protected static string $slug = 'listings';

    protected static bool $timestamps = true;

    protected static string $label = 'Property';
    protected static string $pluralLabel = 'Listings';
    protected static ?string $icon = 'list';

    // How Create and Edit open: 'page' (full page), 'modal' (dialog) or 'slide-over'
    protected static string $formMode = 'page';

    public function form(Form $form): Form
    {
        return $form->schema([
            TextInput::make('title')->translatable()->rules('required|max:190')->columnSpan(2),
            // Related records (see docs/relationships.md in flocms-admin):
            // \FloCMS\Admin\Form\Fields\BelongsTo::make('category_id')->resource(CategoryResource::class),
            // \FloCMS\Admin\Form\Fields\BelongsToMany::make('tags')->resource(TagResource::class)->pivot('properties_tags', 'item_id', 'tag_id'),
        ]);
    }

    public function table(Table $table): Table
    {
        return $table
            ->columns([
                TextColumn::make('id')->sortable()->toggleable(),   // ->toggleable(): listed in the Columns menu
                TextColumn::make('title')->searchable()->sortable(),
                DateColumn::make('updated_at')->sortable()->toggleable(),
            ])
            ->defaultSort('id', 'desc');
    }
}
