ALTER TABLE project
    ADD COLUMN is_virtual bool not null default false;

INSERT INTO project
VALUES ('minecraft',
        'Minecraft',
        '', '', '', false,
        'mod',
        '{}',
        null,
        default,
        false,
        'minecraft',
        true);

INSERT INTO project_version
VALUES (default, 'minecraft', null, '');

UPDATE project_item
SET version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL)
WHERE version_id IS NULL;

UPDATE project_tag
SET version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL)
WHERE version_id IS NULL;

UPDATE recipe_type
SET version_id = (SELECT id FROM project_version WHERE project_id = 'minecraft' AND name IS NULL)
WHERE loc LIKE 'minecraft:%';

ALTER TABLE project_item ALTER COLUMN version_id SET NOT NULL;
ALTER TABLE project_tag ALTER COLUMN version_id SET NOT NULL;
ALTER TABLE recipe_type ALTER COLUMN version_id SET NOT NULL;

DROP INDEX unique_item_no_project;
DROP INDEX unique_tag_no_project;
DROP INDEX unique_recipe_type_no_project;
DROP INDEX unique_recipe_no_project;

