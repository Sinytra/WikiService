CREATE TABLE project_issue
(
    id            varchar(28) PRIMARY KEY,
    level         varchar(255)                           not null,
    deployment_id varchar(28)                            NOT NULL REFERENCES deployment (id) ON DELETE CASCADE,

    type          varchar(255)                           NOT NULL,
    subject       varchar(255)                           NOT NULL,
    details       text,
    file          text,
    version_name  varchar(255),

    created_at    timestamp(3) default CURRENT_TIMESTAMP not null
);

CREATE OR REPLACE TRIGGER project_issue_set_id
    BEFORE INSERT
    ON project_issue
    FOR EACH ROW
EXECUTE FUNCTION set_random_id(28);