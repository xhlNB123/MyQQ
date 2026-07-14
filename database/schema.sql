-- =====================================================================
-- MyQQ 简易聊天工具 —— 数据库建库/建表脚本
-- 数据库：SQL Server（可按需改为 MySQL/SQLite）
-- 表：Users / Friends / Star / BloodType / FriendshipPolicy
--     / Messages / MessageType
-- =====================================================================

-- ---------- 数据库 ----------
-- CREATE DATABASE MyQQ;
-- GO
-- USE MyQQ;
-- GO

-- ---------- 字典表：星座 ----------
CREATE TABLE Star (
    StarId      INT          PRIMARY KEY,
    StarName    NVARCHAR(20) NOT NULL          -- 如：白羊座
);

-- ---------- 字典表：血型 ----------
CREATE TABLE BloodType (
    BloodTypeId   INT          PRIMARY KEY,
    BloodTypeName NVARCHAR(10) NOT NULL         -- 如：A / B / O / AB
);

-- ---------- 字典表：好友策略 ----------
CREATE TABLE FriendshipPolicy (
    PolicyId    INT           PRIMARY KEY,
    PolicyName  NVARCHAR(30)  NOT NULL          -- 如：允许任何人添加 / 需要验证 / 拒绝任何人
);

-- ---------- 字典表：消息类型 ----------
CREATE TABLE MessageType (
    TypeId      INT           PRIMARY KEY,
    TypeName    NVARCHAR(20)  NOT NULL          -- 如：文本 / 系统消息 / 好友请求
);

-- ---------- 用户表 ----------
CREATE TABLE Users (
    UserId      INT IDENTITY(10001,1) PRIMARY KEY,   -- QQ 号
    Account     NVARCHAR(50)  NOT NULL UNIQUE,        -- 登录账号
    Password    NVARCHAR(128) NOT NULL,              -- 密码（建议存哈希）
    NickName    NVARCHAR(50)  NOT NULL,
    Gender      TINYINT       DEFAULT 0,             -- 0 未知 1 男 2 女
    StarId      INT           NULL,
    BloodTypeId INT           NULL,
    PolicyId    INT           DEFAULT 2,             -- 好友添加策略（默认需验证）
    AvatarPath  NVARCHAR(200) NULL,                  -- 头像
    Signature   NVARCHAR(200) NULL,                  -- 个性签名
    Status      TINYINT       DEFAULT 0,             -- 0 离线 1 在线 2 忙碌
    CreateTime  DATETIME      DEFAULT GETDATE(),
    CONSTRAINT FK_Users_Star     FOREIGN KEY (StarId)      REFERENCES Star(StarId),
    CONSTRAINT FK_Users_Blood    FOREIGN KEY (BloodTypeId) REFERENCES BloodType(BloodTypeId),
    CONSTRAINT FK_Users_Policy   FOREIGN KEY (PolicyId)    REFERENCES FriendshipPolicy(PolicyId)
);

-- ---------- 好友关系表 ----------
CREATE TABLE Friends (
    Id          INT IDENTITY(1,1) PRIMARY KEY,
    UserId      INT           NOT NULL,             -- 所属用户
    FriendId    INT           NOT NULL,             -- 好友用户
    Remark      NVARCHAR(50)  NULL,                 -- 备注名
    GroupName   NVARCHAR(50)  DEFAULT N'我的好友',   -- 分组
    AddTime     DATETIME      DEFAULT GETDATE(),
    CONSTRAINT FK_Friends_User   FOREIGN KEY (UserId)   REFERENCES Users(UserId),
    CONSTRAINT FK_Friends_Friend FOREIGN KEY (FriendId) REFERENCES Users(UserId),
    CONSTRAINT UQ_Friends UNIQUE (UserId, FriendId)
);

-- ---------- 消息表 ----------
CREATE TABLE Messages (
    MsgId       INT IDENTITY(1,1) PRIMARY KEY,
    SenderId    INT           NOT NULL,
    ReceiverId  INT           NOT NULL,
    TypeId      INT           NOT NULL,
    Content     NVARCHAR(2000) NOT NULL,
    SendTime    DATETIME      DEFAULT GETDATE(),
    IsRead      BIT           DEFAULT 0,
    FileId      INT           NULL,                 -- 图片/文件消息关联 Files
    CONSTRAINT FK_Msg_Sender   FOREIGN KEY (SenderId)   REFERENCES Users(UserId),
    CONSTRAINT FK_Msg_Receiver FOREIGN KEY (ReceiverId) REFERENCES Users(UserId),
    CONSTRAINT FK_Msg_Type     FOREIGN KEY (TypeId)     REFERENCES MessageType(TypeId)
);

-- ---------- 文件/图片存储表 ----------
CREATE TABLE Files (
    FileId      INT IDENTITY(1,1) PRIMARY KEY,
    OwnerId     INT            NOT NULL,             -- 上传者
    FileName    NVARCHAR(260)  NOT NULL,
    FileSize    BIGINT         NOT NULL,
    Kind        TINYINT        NOT NULL,             -- 1 图片 2 文件
    StorePath   NVARCHAR(400)  NOT NULL,             -- 服务端落盘路径
    CreateTime  DATETIME       DEFAULT GETDATE(),
    CONSTRAINT FK_Files_Owner FOREIGN KEY (OwnerId) REFERENCES Users(UserId)
);

-- ---------- 好友申请表（好友关系仅在接受后写入 Friends）----------
CREATE TABLE FriendRequests (
    RequestId          INT IDENTITY(1,1) PRIMARY KEY,
    SenderId           INT            NOT NULL,
    ReceiverId         INT            NOT NULL,
    VerifyText         NVARCHAR(200)  NOT NULL DEFAULT N'',
    Status             TINYINT        NOT NULL DEFAULT 0,   -- 0 待处理 1 接受 2 拒绝
    CreatedTime        DATETIME       NOT NULL DEFAULT GETDATE(),
    HandledTime        DATETIME       NULL,
    ResultAcknowledged BIT            NOT NULL DEFAULT 0,
    CONSTRAINT CK_FR_NotSelf CHECK (SenderId <> ReceiverId),
    CONSTRAINT FK_FR_Sender   FOREIGN KEY (SenderId)   REFERENCES Users(UserId),
    CONSTRAINT FK_FR_Receiver FOREIGN KEY (ReceiverId) REFERENCES Users(UserId)
);
