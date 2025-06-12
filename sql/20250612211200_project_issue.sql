CREATE TABLE project_issue
(
    id            varchar(28) PRIMARY KEY,
    project_id    text                                   NOT NULL REFERENCES project (id) ON DELETE CASCADE,
    level         varchar(255)                           not null,
    page_path     text,
    deployment_id varchar(28)                            NOT NULL REFERENCES deployment (id) ON DELETE CASCADE,
    body          jsonb                                  not null,
    created_at    timestamp(3) default CURRENT_TIMESTAMP not null
);

CREATE OR REPLACE TRIGGER project_issue_set_id
    BEFORE INSERT
    ON project_issue
    FOR EACH ROW
EXECUTE FUNCTION set_random_id(28);