CREATE TABLE report
(
    id           varchar(28) PRIMARY KEY,

    type         varchar(255)                           NOT NULL,
    reason       text                                   NOT NULL,
    body         text                                   NOT NULL,
    status       varchar(255)                           NOT NULL,

    submitter_id text                                   NOT NULL, -- Non-binding user id
    project_id   text                                   NOT NULL, -- Non-binding project id

    path         text,
    locale       text,
    version_id   bigint,                                          -- Non-binding project_version id

    created_at   timestamp(3) default CURRENT_TIMESTAMP not null
);

CREATE OR REPLACE TRIGGER report_set_id
    BEFORE INSERT
    ON report
    FOR EACH ROW
EXECUTE FUNCTION set_random_id(28);
