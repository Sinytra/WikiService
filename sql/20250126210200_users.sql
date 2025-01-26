create table user_
(
    id          text                                   not null
        primary key,
    modrinth_id text,
    created_at  timestamp(3) default CURRENT_TIMESTAMP not null
);

CREATE TABLE user_project
(
    user_id    text REFERENCES user_ (id) ON DELETE CASCADE,
    project_id text REFERENCES project (id) ON DELETE CASCADE,
    role       varchar(255) NOT NULL,
    CONSTRAINT user_project_pkey PRIMARY KEY (user_id, project_id)
);