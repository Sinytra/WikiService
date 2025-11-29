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

INSERT INTO project_version VALUES (default, 'minecraft', null, '');