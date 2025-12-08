-- Vanilla recipe types
INSERT INTO recipe_type (loc, version_id)
SELECT v.loc, ver.id
FROM (VALUES ('minecraft:crafting_shaped'),
             ('minecraft:crafting_shapeless'),
             ('minecraft:smelting'),
             ('minecraft:blasting'),
             ('minecraft:campfire_cooking'),
             ('minecraft:smoking'),
             ('minecraft:stonecutting'),
             ('minecraft:smithing_transform')) AS v(loc)
         CROSS JOIN
     (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL) AS ver;

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
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:furnace')
WHERE r.loc IN (
    'minecraft:smelting'
    )
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:blast_furnace')
WHERE r.loc IN (
    'minecraft:blasting'
    )
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

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
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:smoker')
WHERE r.loc IN (
    'minecraft:smoking'
    )
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:stonecutter')
WHERE r.loc IN (
    'minecraft:stonecutting'
    )
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);

INSERT INTO recipe_workbench (type_id, item_id)
SELECT r.id, pitem.id
FROM recipe_type r
         JOIN project_item pitem ON pitem.item_id = (SELECT id FROM item i WHERE i.loc = 'minecraft:smithing_table')
WHERE r.loc IN (
    'minecraft:smithing_transform'
    )
  AND r.version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL);
