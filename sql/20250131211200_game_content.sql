CREATE TABLE item_id
(
    id varchar(255) primary key
);

CREATE TABLE item
(
    id         bigserial primary key,
    item_id    varchar(255) REFERENCES item_id (id) ON DELETE CASCADE,
    project_id text references project (id) ON DELETE CASCADE
);

CREATE UNIQUE INDEX unique_item_project_not_null
    ON item (item_id, project_id);

CREATE UNIQUE INDEX unique_item_no_project
    ON item (item_id)
    WHERE project_id IS NULL;

CREATE TABLE tag_id
(
    id varchar(255) primary key
);

CREATE TABLE tag
(
    id         bigserial primary key,
    tag_id     varchar(255) REFERENCES tag_id (id) ON DELETE CASCADE,
    project_id text references project (id) ON DELETE CASCADE,

    UNIQUE (tag_id, project_id)
);

CREATE UNIQUE INDEX unique_tag_project_not_null
    ON tag (tag_id, project_id);

CREATE UNIQUE INDEX unique_tag_no_project
    ON tag (tag_id)
    WHERE project_id IS NULL;

CREATE TABLE tag_item
(
    tag_id  bigint REFERENCES tag (id) ON DELETE CASCADE,
    item_id bigint REFERENCES item (id) ON DELETE CASCADE,

    PRIMARY KEY (tag_id, item_id)
);

CREATE TABLE tag_tag
(
    parent bigint REFERENCES tag (id) ON DELETE CASCADE,
    child  bigint REFERENCES tag (id) ON DELETE CASCADE,

    PRIMARY KEY (parent, child),
    CHECK (parent <> child)
);

CREATE FUNCTION tags_insert_trigger_func() RETURNS trigger AS
$BODY$
DECLARE
    results bigint;
BEGIN
    WITH RECURSIVE p(id) AS (SELECT parent
                             FROM tag_tag
                             WHERE child = NEW.parent
                             UNION
                             SELECT parent
                             FROM p,
                                  tag_tag d
                             WHERE p.id = d.child)
    SELECT *
    INTO results
    FROM p
    WHERE id = NEW.child;

    IF FOUND THEN
        RAISE EXCEPTION 'Circular dependencies are not allowed.';
    END IF;
    RETURN NEW;
END;
$BODY$ LANGUAGE plpgsql;

CREATE TRIGGER before_insert_tag_tag_trg
    BEFORE INSERT
    ON tag_tag
    FOR EACH ROW
EXECUTE PROCEDURE tags_insert_trigger_func();

CREATE TABLE recipe
(
    id         bigserial primary key,
    project_id text references project (id) ON DELETE CASCADE not null,
    loc        varchar(255)                                   not null,
    type       varchar(255)                                   not null,

    UNIQUE (project_id, loc)
);

CREATE TABLE recipe_ingredient_tag
(
    recipe_id bigserial REFERENCES recipe (id) ON DELETE CASCADE,
    tag_id    varchar(255) REFERENCES tag_id (id) ON DELETE CASCADE,
    slot      int  not null,
    count     int  not null,
    input     bool not null,

    PRIMARY KEY (recipe_id, tag_id, slot, input)
);

CREATE TABLE recipe_ingredient_item
(
    recipe_id bigserial REFERENCES recipe (id) ON DELETE CASCADE,
    item_id   varchar(255) REFERENCES item_id (id) ON DELETE CASCADE,
    slot      int  not null,
    count     int  not null,
    input     bool not null,

    PRIMARY KEY (recipe_id, item_id, slot, input)
);