from __future__ import annotations

from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile
from xml.sax.saxutils import escape

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "BeamDrop_课程设计报告.docx"
BUILD = ROOT / "docs" / "course_report_build"
BUILD.mkdir(exist_ok=True)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    paths = [
        r"C:\Windows\Fonts\msyhbd.ttc" if bold else r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf" if bold else r"C:\Windows\Fonts\simsun.ttc",
    ]
    for p in paths:
        if Path(p).exists():
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def architecture_png(path: Path) -> None:
    im = Image.new("RGB", (1600, 570), "#F7F9FC")
    d = ImageDraw.Draw(im)
    f1, f2 = font(30, True), font(22)
    boxes = [(55, 190, 330, 360, "React GUI", "接收服务、发送任务、进度展示"),
             (420, 190, 710, 360, "FastAPI 控制层", "TaskManager · EventBus · REST/WS"),
             (800, 190, 1085, 360, "pybind11", "NativeGateway"),
             (1175, 190, 1545, 360, "C++ 传输核心", "协议 · TCP 网络 · 文件系统")]
    d.text((55, 55), "BeamDrop 系统总体架构", font=font(38, True), fill="#12355B")
    for x1, y1, x2, y2, title, sub in boxes:
        d.rounded_rectangle((x1, y1, x2, y2), 18, fill="#E7F2FA", outline="#2486B9", width=3)
        d.text((x1 + 18, y1 + 40), title, font=f1, fill="#103C5D")
        d.multiline_text((x1 + 18, y1 + 95), sub, font=f2, fill="#334E68", spacing=8)
    for i in range(3):
        x = boxes[i][2] + 12
        y = 275
        d.line((x, y, boxes[i + 1][0] - 15, y), fill="#2486B9", width=5)
        d.polygon([(boxes[i + 1][0] - 15, y), (boxes[i + 1][0] - 30, y - 9), (boxes[i + 1][0] - 30, y + 9)], fill="#2486B9")
    d.text((360, 445), "控制面：REST 权威快照 + WebSocket 增量事件", font=font(25), fill="#4C6272")
    d.text((430, 490), "数据面：真实 C++ TCP 文件传输与 SHA256 完整性校验", font=font(25), fill="#4C6272")
    im.save(path)


def flow_png(path: Path) -> None:
    im = Image.new("RGB", (1600, 760), "#FFFFFF")
    d = ImageDraw.Draw(im)
    d.text((60, 42), "文件传输与断点续传流程", font=font(38, True), fill="#12355B")
    steps = ["HELLO\n会话清单", "FILE_INFO\n路径、大小、SHA256", "RESUME_ACK\n返回安全 offset", "DATA\n分块传输", "FILE_END\n计算 SHA256", "FINISH\n任务结束"]
    x = 60
    for i, text in enumerate(steps):
        d.rounded_rectangle((x, 180, x + 215, 350), 16, fill="#E7F2FA", outline="#2486B9", width=3)
        d.multiline_text((x + 20, 225), text, font=font(24, True), fill="#103C5D", spacing=10)
        if i < len(steps) - 1:
            d.line((x + 217, 265, x + 270, 265), fill="#2486B9", width=5)
            d.polygon([(x + 270, 265), (x + 255, 256), (x + 255, 274)], fill="#2486B9")
        x += 270
    d.rounded_rectangle((90, 455, 1510, 660), 18, fill="#F3F7F9", outline="#8AA6B7", width=2)
    d.text((125, 485), "接收端安全策略", font=font(29, True), fill="#12355B")
    lines = ["• Packet Header：Magic=BDRP、Version、Type、Flags、Length、Checksum，固定 52 字节。",
             "• 对 manifest、JSON 元数据和 offset 严格校验；offset 与本地文件大小冲突时选择保守位置。",
             "• FILE_END 后验证文件大小与 SHA256；失败发送 ERROR 并记录日志；路径处理防止路径穿越。"]
    y = 540
    for line in lines:
        d.text((125, y), line, font=font(22), fill="#334E68")
        y += 42
    im.save(path)


def result_png(path: Path) -> None:
    im = Image.new("RGB", (1600, 670), "#F7F9FC")
    d = ImageDraw.Draw(im)
    d.text((60, 45), "实验验证结果", font=font(38, True), fill="#12355B")
    rows = [("Windows C++ 核心", "CTest 3/3 通过"), ("Python 后端", "pytest 31/31 通过"),
            ("React 前端", "test 10/10、lint、build 通过"),
            ("Linux 端到端", "普通/空文件/中文文件名、SHA256、失败语义、停止服务通过")]
    y = 135
    for i, (item, result) in enumerate(rows):
        fill = "#E7F2FA" if i % 2 == 0 else "#FFFFFF"
        d.rounded_rectangle((70, y, 1530, y + 100), 12, fill=fill, outline="#B6CDD9", width=2)
        d.text((105, y + 30), item, font=font(25, True), fill="#103C5D")
        d.text((570, y + 32), "✓  " + result, font=font(23), fill="#18794E")
        y += 115
    d.text((72, 610), "验证依据：项目已有测试记录与答辩汇报稿；测试结果用于说明当前版本的可验证范围。", font=font(19), fill="#4C6272")
    im.save(path)


def p(text: str, style: str = "Normal", bold: bool = False, center: bool = False) -> str:
    prop = '<w:pPr><w:pStyle w:val="%s"/>' % style
    if center: prop += '<w:jc w:val="center"/>'
    prop += '</w:pPr>'
    rpr = '<w:rPr><w:b/></w:rPr>' if bold else ''
    return f'<w:p>{prop}<w:r>{rpr}<w:t xml:space="preserve">{escape(text)}</w:t></w:r></w:p>'


def image(rel: str, title: str) -> str:
    return f'''<w:p><w:pPr><w:jc w:val="center"/></w:pPr><w:r><w:drawing><wp:inline distT="0" distB="0" distL="0" distR="0" xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"><wp:extent cx="5486400" cy="2057400"/><wp:docPr id="1" name="{title}"/><a:graphic xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:pic xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:nvPicPr><pic:cNvPr id="0" name="{title}.png"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed="{rel}" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="5486400" cy="2057400"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>'''


def table(rows: list[tuple[str, str]]) -> str:
    xml = '<w:tbl><w:tblPr><w:tblW w:w="0" w:type="auto"/><w:tblBorders><w:top w:val="single" w:sz="8"/><w:left w:val="single" w:sz="8"/><w:bottom w:val="single" w:sz="8"/><w:right w:val="single" w:sz="8"/><w:insideH w:val="single" w:sz="4"/><w:insideV w:val="single" w:sz="4"/></w:tblBorders></w:tblPr>'
    for a, b in rows:
        xml += '<w:tr>' + ''.join(f'<w:tc><w:tcPr><w:tcW w:w="4500" w:type="dxa"/></w:tcPr>{p(x)}</w:tc>' for x in (a,b)) + '</w:tr>'
    return xml + '</w:tbl>'


def main() -> None:
    arch, flow, result = (BUILD / 'architecture.png', BUILD / 'flow.png', BUILD / 'result.png')
    architecture_png(arch); flow_png(flow); result_png(result)
    body = [
        p('课程设计报告', 'Title', center=True), p('项目名称：BeamDrop（邻光）局域网文件传输系统', 'Subtitle', center=True),
        p('课程名称：____________________    班级：____________________', center=True), p('学生姓名：____________________    学号：____________________', center=True),
        p('指导教师：____________________    完成日期：2026 年 7 月', center=True), '<w:p><w:r><w:br w:type="page"/></w:r></w:p>',
        p('摘  要', 'Heading1'), p('BeamDrop（邻光）是一个面向同一局域网场景的文件传输系统。项目以 C++ 实现 TCP 传输核心，采用 React 构建浏览器图形界面，使用 FastAPI 提供控制 API，并通过 pybind11 连接控制面与原生传输能力。系统实现了接收服务管理、路径式单文件发送、进度与终态展示、SHA256 完整性校验、结构化失败反馈和状态恢复。本报告围绕选题分析、设计方案、关键问题及解决方法、实验结果和课程学习总结展开。'),
        p('关键词：局域网文件传输；TCP；C++；FastAPI；React；SHA256；断点续传', 'Normal'),
        p('1 课程设计选题内容与分析', 'Heading1'), p('1.1 选题背景', 'Heading2'), p('日常局域网文件交换常常只关注“能否传输”，却忽略服务状态、传输进度、失败原因和内容一致性。课程设计选择局域网文件传输系统，目标是在可控范围内完成从发送到接收、校验和展示的真实闭环，而不是构造仅有界面的模拟程序。'),
        p('1.2 功能需求', 'Heading2'), table([('需求项','实现或设计要点'),('文件传输','通过 TCP 在客户端与接收端之间传递文件；核心架构为 Client/Server。'),('可观察性','展示接收服务状态、任务状态、进度、已传字节和结构化错误。'),('完整性','发送前记录文件 SHA256，接收端在 FILE_END 后验证大小和 SHA256。'),('可靠性','协议预留 RESUME_REQ/RESUME_ACK；接收端根据已确认 offset 实现安全续传。'),('可维护性','网络、协议、传输、文件系统和应用编排按职责分层。')]),
        p('1.3 范围与约束', 'Heading2'), p('当前可演示版本重点支持本机 GUI 管理接收服务和路径式单文件发送。自动发现、认证加密、公网部署、多客户端并发接收、桌面安装包和任务历史数据库不属于本阶段交付范围。浏览器输入本机绝对路径是本机控制面的明确边界，不等同于将文件上传至网页临时目录。'),
        p('2 设计方案', 'Heading1'), p('2.1 总体架构', 'Heading2'), p('系统分为控制面和数据面：React 负责交互；FastAPI 管理任务、接收服务和事件；pybind11 负责原生调用桥接；C++ 核心完成协议处理、TCP 通信和文件写入。REST /api/snapshot 提供权威状态快照，WebSocket 推送实时增量事件，页面刷新或重连后可由快照恢复状态。'), image('rId2','总体架构'), p('图 1 BeamDrop 分层总体架构', center=True),
        p('2.2 C++ 核心模块设计', 'Heading2'), table([('模块','职责'),('network','TCP 连接、监听与字节读写；不理解文件业务。'),('protocol','Packet Header、序列化、解析及 magic/version/length/checksum 校验。'),('transfer','发送、接收、会话组织、断点续传与进度上报。'),('filesystem','扫描文件、相对路径处理、目录创建、读写和路径穿越防护。'),('app / cli','组合配置、日志和各核心模块，向 CLI 或上层控制面提供入口。')]),
        p('2.3 协议与传输流程', 'Heading2'), p('协议采用“固定二进制 Header + JSON 或二进制 Payload”。Header 共 52 字节，包含 Magic（BDRP）、Version、Type、Flags、Length 和 32 字节 Checksum。会话依次使用 HELLO、FILE_INFO、RESUME_ACK、DATA、FILE_END、FINISH 等消息。FILE_INFO 包含 relative_path、size、sha256；接收端只接受合法字段，并拒绝未知版本和非法数字。'), image('rId3','传输流程'), p('图 2 文件传输、校验与续传流程', center=True),
        p('2.4 状态与错误设计', 'Heading2'), p('发送任务遵循 pending → running → completed / failed 的状态机。对不可达端口等网络错误，系统应以 NETWORK_FAILURE 等结构化错误落入 failed 终态，避免任务长期停留在 running。接收服务的启动和停止同样以底层实际状态为依据，只有收敛为 stopped 才向界面报告停止成功。'),
        p('3 问题及解决方法', 'Heading1'), table([('问题','解决方法'),('TCP 是字节流，无法天然区分消息边界','定义固定长度包头，以 Length 确定 payload 边界，并检查 Magic、版本和校验字段。'),('中断后重复传输成本高','由接收端根据 transfer_state 与本地文件确定 RESUME_ACK.offset，客户端从安全 offset 继续发送。'),('文件正确性难以确认','对完整文件使用 SHA256；FILE_END 后重新计算接收文件哈希，失败则发送 ERROR 并记录日志。'),('恶意路径可能写出保存目录','filesystem 层生成相对路径、创建目录并防止路径穿越。'),('前端事件可能丢失','将 REST 快照定义为权威状态，WebSocket 仅传输增量；刷新和重连时重新拉取快照。'),('原生传输能力不易操作','通过 FastAPI 与 pybind11 建立控制面，既保留 C++ 网络能力，又提供 GUI 可观察性。')]),
        p('4 实验结果展示', 'Heading1'), p('4.1 测试环境与方法', 'Heading2'), p('Windows 演示环境使用前端 http://127.0.0.1:5173、后端 http://127.0.0.1:8000 和接收端口 9090。测试覆盖 C++ 核心、Python 后端、React 前端以及 Linux 端到端场景。端到端验证包括普通文件、空文件、中文文件名、源/目标 SHA256 对比、不可达端口失败语义、页面刷新恢复和接收服务停止。'), image('rId4','实验结果'), p('图 3 自动化与端到端验证结果汇总', center=True),
        p('4.2 结果分析', 'Heading2'), p('已有测试记录显示：Windows C++ 核心 CTest 3/3 通过；Python 后端 pytest 31/31 通过；React 前端 test 10/10、lint 和 build 通过；Linux 端到端场景验证通过。结果表明项目的核心传输、控制 API、前端状态展示和典型边界数据能够形成可验证闭环。SHA256 一致性验证是传输成功的关键证据；不可达端口进入 failed 状态则验证了失败语义。'),
        p('5 本课程学习总结', 'Heading1'), p('本课程设计将网络编程、文件系统、协议设计、跨语言绑定、Web API 和前端状态管理结合到一个完整项目中。实现过程说明，可靠的传输系统不能只依赖 socket 的连通性，还必须定义清晰的消息边界、错误语义、状态机和完整性校验。通过协议层与网络层解耦、传输流程与 GUI/CLI 解耦，系统获得了较好的可维护性与扩展性。'),
        p('后续工作将围绕 UDP 自动发现、多文件/目录并发、身份认证与加密、桌面端封装、任务持久化和更完整的性能测试展开。当前版本的限制已被明确记录，后续扩展应在不破坏现有协议校验、状态语义和测试闭环的前提下进行。'),
        p('参考文献', 'Heading1'), p('[1] BeamDrop 项目 README：构建、运行与测试说明。'), p('[2] BeamDrop 项目文档《需求分析》《总体设计》《协议设计》《PPT汇报稿》。'), p('[3] RFC 793. Transmission Control Protocol. Internet Engineering Task Force.'),
    ]
    document = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><w:body>''' + ''.join(body) + '<w:sectPr><w:pgSz w:w="11906" w:h="16838"/><w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440"/></w:sectPr></w:body></w:document>'
    styles = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="Microsoft YaHei"/><w:sz w:val="22"/></w:rPr></w:rPrDefault></w:docDefaults><w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:pPr><w:spacing w:line="360" w:lineRule="auto" w:after="140"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Title"><w:name w:val="Title"/><w:rPr><w:b/><w:sz w:val="44"/></w:rPr><w:pPr><w:spacing w:before="800" w:after="600"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Subtitle"><w:name w:val="Subtitle"/><w:rPr><w:sz w:val="28"/></w:rPr><w:pPr><w:spacing w:after="500"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:rPr><w:b/><w:sz w:val="32"/></w:rPr><w:pPr><w:spacing w:before="350" w:after="180"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/><w:rPr><w:b/><w:sz w:val="26"/></w:rPr><w:pPr><w:spacing w:before="240" w:after="120"/></w:pPr></w:style></w:styles>'''
    content = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/></Types>'''
    rels = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>'''
    drels = '''<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/architecture.png"/><Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/flow.png"/><Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="media/result.png"/></Relationships>'''
    with ZipFile(OUT, 'w', ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content); z.writestr('_rels/.rels', rels)
        z.writestr('word/document.xml', document); z.writestr('word/styles.xml', styles); z.writestr('word/_rels/document.xml.rels', drels)
        z.write(arch, 'word/media/architecture.png'); z.write(flow, 'word/media/flow.png'); z.write(result, 'word/media/result.png')
    print(OUT)


if __name__ == '__main__':
    main()