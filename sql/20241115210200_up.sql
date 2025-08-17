create table project
(
    id            text                                   not null
        primary key,
    name          text                                   not null,
    source_path   text                                   not null,
    source_repo   text                                   not null,
    source_branch text                                   not null,
    is_community  boolean      default false             not null,
    type          varchar(255)                           not null,
    platforms     varchar(255)                           not null,
    search_vector tsvector,
    created_at    timestamp(3) default CURRENT_TIMESTAMP not null,
    is_public     boolean      default false             not null,
    modid         varchar(255)                           not null
);

CREATE UNIQUE INDEX "project_source_repo_source_path_key"
    on project (source_repo, source_path);

CREATE INDEX project_search_vector_idx ON project USING gin (search_vector);

CREATE OR REPLACE FUNCTION update_search_vector()
    RETURNS TRIGGER AS
$$
BEGIN
    NEW.search_vector := to_tsvector('simple', NEW.id || ' ' || NEW.name);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_search_vector
    BEFORE INSERT
    ON project
    FOR EACH ROW
EXECUTE FUNCTION update_search_vector();

CREATE DOMAIN resource_location AS varchar(255)
    CHECK ( VALUE ~* '^([a-z0-9_.-]+:)?[a-z0-9_.-\/]+$' );