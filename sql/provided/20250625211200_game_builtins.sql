-- Vanilla recipe types
INSERT INTO recipe_type (loc)
VALUES ('minecraft:crafting_shaped'),
       ('minecraft:crafting_shapeless'),
       ('minecraft:smelting'),
       ('minecraft:blasting'),
       ('minecraft:campfire_cooking');

-- Vanilla recipe workbenches
INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id IN
                                    (SELECT id
                                     FROM item i
                                     WHERE i.loc IN (
                                                     'minecraft:crafting_table',
                                                     'minecraft:crafter'
                                         ))
WHERE r.loc IN (
                'minecraft:crafting_shaped',
                'minecraft:crafting_shapeless'
    )
  AND r.version_id IS NULL;

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:furnace')
WHERE r.loc IN (
    'minecraft:smelting'
    )
  AND r.version_id IS NULL;

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:blast_furnace')
WHERE r.loc IN (
    'minecraft:blasting'
    )
  AND r.version_id IS NULL;

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id IN
                                    (SELECT id
                                     FROM item i
                                     WHERE i.loc IN
                                           (
                                               'minecraft:campfire',
                                               'minecraft:soul_campfire'
                                               ))
WHERE r.loc IN (
    'minecraft:campfire_cooking'
    )
  AND r.version_id IS NULL;
