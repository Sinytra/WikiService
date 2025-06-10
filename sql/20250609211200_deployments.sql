CREATE OR REPLACE FUNCTION set_random_id()
    RETURNS TRIGGER AS
$$
DECLARE
    _len   int := COALESCE((TG_ARGV[0])::int, 28);
    _id    text;
    _taken bool;
BEGIN
    LOOP
        SELECT string_agg(
                       substr('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789',
                              ceil(random() * 62)::int, 1), '')
        INTO _id
        FROM generate_series(1, _len);

        EXECUTE format('SELECT EXISTS (SELECT 1 FROM %I.%I WHERE id = $1)',
                       TG_TABLE_SCHEMA, TG_TABLE_NAME)
            USING _id
            INTO _taken;

        EXIT WHEN NOT _taken;
    END LOOP;

    NEW.id := _id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TABLE deployment
(
    id         varchar(28) PRIMARY KEY,
    project_id text         NOT NULL REFERENCES project (id) ON DELETE CASCADE,
    revision   jsonb,
    status     varchar(255) NOT NULL,
    active     bool         NOT NULL DEFAULT FALSE,
    user_id    text references user_ (id),
    created_at timestamp(3)          default CURRENT_TIMESTAMP NOT NULL
);

CREATE OR REPLACE TRIGGER deployment_set_id
    BEFORE INSERT
    ON deployment
    FOR EACH ROW
EXECUTE FUNCTION set_random_id(28);

CREATE UNIQUE INDEX single_active_deployment ON deployment (project_id, active) WHERE active;