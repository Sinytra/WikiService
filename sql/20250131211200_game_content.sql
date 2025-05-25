CREATE TABLE project_version
(
    id bigserial PRIMARY KEY,
    project_id text NOT NULL REFERENCES project (id) ON DELETE CASCADE,
    name varchar(255),
    branch varchar(255) NOT NULL,
--    game_version varchar(255) NOT NULL

    UNIQUE (project_id, name, branch)
);

CREATE TABLE item
(
    id  bigserial PRIMARY KEY,
    loc varchar(255) NOT NULL CHECK (loc <> ''),

    UNIQUE (loc)
);

CREATE TABLE project_item
(
    id         bigserial PRIMARY KEY,
    item_id    bigint NOT NULL REFERENCES item (id) ON DELETE CASCADE,
    version_id bigint REFERENCES project_version (id) ON DELETE CASCADE,

    UNIQUE (item_id, version_id)
);

CREATE UNIQUE INDEX unique_item_no_project
    ON project_item (item_id)
    WHERE version_id IS NULL;

CREATE TABLE project_item_page
(
    item_id   bigint NOT NULL REFERENCES project_item (id) ON DELETE CASCADE,
    path text NOT NULL
);

CREATE TABLE tag
(
    id  bigserial PRIMARY KEY,
    loc varchar(255),

    UNIQUE (loc)
);

CREATE TABLE project_tag
(
    id         bigserial PRIMARY KEY,
    tag_id     bigint NOT NULL REFERENCES tag (id) ON DELETE CASCADE,
    version_id bigint REFERENCES project_version (id) ON DELETE CASCADE,

    UNIQUE (tag_id, version_id)
);

CREATE UNIQUE INDEX unique_tag_no_project
    ON project_tag (tag_id)
    WHERE version_id IS NULL;

CREATE TABLE tag_item
(
    tag_id  bigint NOT NULL REFERENCES project_tag (id) ON DELETE CASCADE,
    item_id bigint NOT NULL REFERENCES project_item (id) ON DELETE CASCADE,

    PRIMARY KEY (tag_id, item_id)
);

CREATE TABLE tag_tag
(
    parent bigint NOT NULL REFERENCES project_tag (id) ON DELETE CASCADE,
    child  bigint NOT NULL REFERENCES project_tag (id) ON DELETE CASCADE,

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
    id         bigserial PRIMARY KEY,
    version_id bigint REFERENCES project_version (id) ON DELETE CASCADE,
    loc        varchar(255)                                   NOT NULL,
    type       varchar(255)                                   NOT NULL,

    UNIQUE (version_id, loc)
);

CREATE UNIQUE INDEX unique_recipe_no_project
    ON recipe (loc)
    WHERE version_id IS NULL;

CREATE TABLE recipe_ingredient_tag
(
    recipe_id bigserial NOT NULL REFERENCES recipe (id) ON DELETE CASCADE,
    tag_id    bigint NOT NULL REFERENCES tag (id) ON DELETE CASCADE,
    slot      int  NOT NULL,
    count     int  NOT NULL,
    input     bool NOT NULL,

    PRIMARY KEY (recipe_id, tag_id, slot, input)
);

CREATE TABLE recipe_ingredient_item
(
    recipe_id bigserial NOT NULL REFERENCES recipe (id) ON DELETE CASCADE,
    item_id   bigint NOT NULL REFERENCES item (id) ON DELETE CASCADE,
    slot      int  not null,
    count     int  not null,
    input     bool not null,

    PRIMARY KEY (recipe_id, item_id, slot, input)
);

CREATE MATERIALIZED VIEW tag_item_flat AS
SELECT *
FROM (WITH RECURSIVE tag_hierarchy AS (SELECT tp.id                            AS parent,
                                              tc.id                            AS child,
                                              array [ tp.id, tc.id ]::bigint[] AS track
                                       FROM tag_tag
                                                JOIN project_tag tp ON tag_tag.parent = tp.id
                                                JOIN project_tag tc ON tag_tag.child = tc.id
                                       UNION ALL
                                       SELECT tp.id AS parent, tc.id AS child, tag_hierarchy.track || tc.id
                                       FROM tag_tag n
                                                JOIN project_tag tp ON n.parent = tp.id
                                                JOIN project_tag tc ON n.child = tc.id
                                                JOIN tag_hierarchy ON tp.id = tag_hierarchy.child)
      -- Combine tag relationships with direct tag-to-item mappings
      SELECT th.parent AS parent, project_item.id AS child
      FROM tag_hierarchy th
               JOIN project_tag ON project_tag.tag_id = th.child
               JOIN tag_item ti ON project_tag.id = ti.tag_id
               JOIN project_item ON project_item.id = ti.item_id

      UNION ALL

      -- Include direct tag-to-item mappings
      SELECT project_tag.id AS parent, project_item.id AS child
      FROM tag_item
               JOIN project_tag on project_tag.id = tag_item.tag_id
               JOIN project_item ON project_item.id = tag_item.item_id) as subq;