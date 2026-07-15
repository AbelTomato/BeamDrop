# BeamDrop Frontend

React 单页控制面。页面和 feature 层仅依赖 `src/platform/BeamDropApi.ts`；REST、WebSocket 与 FastAPI DTO 转换只存在于 `platform/fastapi/FastApiAdapter.ts`。`dashboardState.ts` 以 REST 快照作为权威状态，并将 WebSocket 事件归约为增量更新。

## 开发

```cmd
npm --prefix frontend run dev -- --host 127.0.0.1 --port 5173
```

默认连接 `http://127.0.0.1:8000`，可通过 `VITE_BEAMDROP_API_URL` 覆盖。页面使用本机路径输入，不上传浏览器文件副本。

```cmd
npm --prefix frontend test
npm --prefix frontend run lint
npm --prefix frontend run build
```

样式使用 Tailwind CSS v4，基础控件采用本地维护的 shadcn/ui 兼容组件，位置在 `src/components/ui/`。未来 Tauri 页面迁移只需在 composition point 替换 `BeamDropApi` adapter。

<!--

Currently, two official plugins are available:

- [@vitejs/plugin-react](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react) uses [Oxc](https://oxc.rs)
- [@vitejs/plugin-react-swc](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react-swc) uses [SWC](https://swc.rs/)

## React Compiler

The React Compiler is not enabled on this template because of its impact on dev & build performances. To add it, see [this documentation](https://react.dev/learn/react-compiler/installation).

## Expanding the Oxlint configuration

If you are developing a production application, we recommend enabling type-aware lint rules by installing `oxlint-tsgolint` and editing `.oxlintrc.json`:

```json
{
  "$schema": "./node_modules/oxlint/configuration_schema.json",
  "plugins": ["react", "typescript", "oxc"],
  "options": {
    "typeAware": true
  },
  "rules": {
    "react/rules-of-hooks": "error",
    "react/only-export-components": ["warn", { "allowConstantExport": true }]
  }
}
```

See the [Oxlint rules documentation](https://oxc.rs/docs/guide/usage/linter/rules) for the full list of rules and categories.
-->
