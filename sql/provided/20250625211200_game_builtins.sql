-- Vanilla recipe types
INSERT INTO recipe_type (loc)
VALUES ('minecraft:crafting_shaped'),
       ('minecraft:crafting_shapeless');

-- Vanilla recipe workbenches
INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:crafting_table')
WHERE r.loc IN (
                'minecraft:crafting_shaped',
                'minecraft:crafting_shapeless'
    )
  AND r.version_id IS NULL;
