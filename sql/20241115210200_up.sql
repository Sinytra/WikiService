create table mod
(
    id            text                                   not null
        primary key,
    name          text                                   not null,
    platform      varchar(50)                            not null,
    slug          text                                   not null,
    "createdAt"   timestamp(3) default CURRENT_TIMESTAMP not null,
    source_path   text                                   not null,
    source_repo   text                                   not null,
    source_branch text                                   not null,
    is_community  boolean      default false             not null
);

create unique index "mod_source_repo_source_path_key"
    on mod (source_repo, source_path);

