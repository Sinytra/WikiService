ALTER TABLE user_ ADD COLUMN role varchar(255) not null default 'user';

CREATE TABLE data_import
(
    id             bigserial PRIMARY KEY,
    game_version   varchar(255)                           not null,
    loader         varchar(255)                           not null,
    loader_version varchar(255)                           not null,
    user_id        text references user_ (id),
    created_at     timestamp(3) default CURRENT_TIMESTAMP not null
);