ALTER TABLE project
    ALTER COLUMN platforms TYPE text,
    ADD COLUMN visibility varchar(255) not null default 'public',
    ADD COLUMN flags text;
