package com.rawlab.android

import android.net.Uri
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.layout.*
import androidx.compose.ui.Alignment
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.ui.semantics.Role
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

class MainActivity : ComponentActivity() {
    val model: EditorViewModel by lazy { ViewModelProvider(this)[EditorViewModel::class.java] }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { RawLabTheme { RawLabApp(model) } }
    }
}

@Composable
fun RawLabTheme(content: @Composable () -> Unit) {
    val scheme = if (isSystemInDarkTheme()) darkColorScheme(
        primary = Color(0xFFE9CE55), onPrimary = Color(0xFF29250A), secondary = Color(0xFF7ED4D3),
        surface = Color(0xFF202122), surfaceContainer = Color(0xFF292A2B), background = Color(0xFF18191A),
    ) else lightColorScheme(primary = Color(0xFF6E5900), secondary = Color(0xFF00696B),
        surface = Color(0xFFF9F9F9), background = Color(0xFFF0F1F2))
    MaterialTheme(colorScheme = scheme, content = content)
}

@Composable
private fun RadioRow(label: String, selected: Boolean, onSelect: () -> Unit) {
    Row(Modifier.fillMaxWidth().selectable(selected = selected, role = Role.RadioButton,
        onClick = onSelect), verticalAlignment = Alignment.CenterVertically) {
        RadioButton(selected = selected, onClick = onSelect)
        Text(label, Modifier.padding(start = 4.dp))
    }
}

@Composable
private fun RawLabApp(model: EditorViewModel) {
    val state by model.state.collectAsStateWithLifecycle()
    var album by rememberSaveable { mutableStateOf(false) }
    var exportDialog by rememberSaveable { mutableStateOf(false) }
    var png by rememberSaveable { mutableStateOf(false) }
    var licenses by rememberSaveable { mutableStateOf(false) }
    var composer by rememberSaveable { mutableStateOf(false) }
    var pendingImport by remember { mutableStateOf<List<Uri>?>(null) }
    var importBase by remember { mutableIntStateOf(0) }  // 0 auto, 1 neutral, 2 std
    val composeState by model.compose.collectAsStateWithLifecycle()
    val openFile = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) { album = false; model.importPhoto(uri) }
    }
    val openLut = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        if (uris.isNotEmpty()) pendingImport = uris
    }
    val saveJpeg = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("image/jpeg")) {
        if (it == null) model.cancelExport() else model.export(it, false)
    }
    val savePng = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("image/png")) {
        if (it == null) model.cancelExport() else model.export(it, true)
    }
    pendingImport?.let { uris ->
        AlertDialog(onDismissRequest = { pendingImport = null },
            title = { Text(stringResource(R.string.import_base_title)) },
            text = {
                Column {
                    RadioRow(stringResource(R.string.import_base_auto), importBase == 0) { importBase = 0 }
                    RadioRow(stringResource(R.string.import_base_neutral), importBase == 1) { importBase = 1 }
                    RadioRow(stringResource(R.string.import_base_std), importBase == 2) { importBase = 2 }
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    model.importLuts(uris, listOf("auto", "neutral", "std")[importBase])
                    pendingImport = null
                }) { Text(stringResource(R.string.confirm)) }
            },
            dismissButton = {
                TextButton(onClick = { pendingImport = null }) { Text(stringResource(R.string.cancel)) }
            })
    }

    if (composer) {
        LutComposerScreen(composeState, model.state.value.userLuts,
            onDismiss = { composer = false },
            onRender = { a, sa, b, sb -> model.composeRender(a, sa, b, sb) },
            onSave = { a, sa, b, sb, name -> model.saveComposedLut(a, sa, b, sb, name) },
            onConsumeSaved = { })
    } else if (album) {
        AlbumScreen(model.storage, onBack = { album = false }, onFile = { openFile.launch(arrayOf("*/*")) },
            onPhoto = { album = false; model.importPhoto(it) })
    } else {
        EditorScreen(state, onAlbum = { album = true }, onFile = { openFile.launch(arrayOf("*/*")) },
            onEdit = model::edit, onReset = model::reset, onRetry = model::retry,
            onExport = { if (model.beginExport()) exportDialog = true },
            onMessageDismiss = model::dismissMessage, onLicenses = { licenses = true }, onGpuChange = model::setGpuEnabled,
            onImportLut = { openLut.launch(arrayOf("*/*")) },
            onDeleteLut = model::deleteLut, onLutMode = model::setUserLutMode,
            onOpenComposer = { composer = true }, onSetLutBase = model::setLutBase)
    }
    if (exportDialog) AlertDialog(
        onDismissRequest = { exportDialog = false; model.cancelExport() },
        title = { Text(stringResource(R.string.export)) },
        text = {
            Column {
                Row {
                    FilterChip(selected = !png, onClick = { png = false }, label = { Text(stringResource(R.string.jpeg)) })
                    Spacer(Modifier.width(8.dp))
                    FilterChip(selected = png, onClick = { png = true }, label = { Text(stringResource(R.string.png)) })
                }
                if (Build.VERSION.SDK_INT >= 29) TextButton(onClick = {
                    exportDialog = false; model.export(null, png)
                }) { Text(stringResource(R.string.save_album)) }
                TextButton(onClick = {
                    exportDialog = false
                    val name = "RawLab-${System.currentTimeMillis()}"
                    if (png) savePng.launch("$name.png") else saveJpeg.launch("$name.jpg")
                }) { Text(stringResource(R.string.save_file)) }
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = { exportDialog = false; model.cancelExport() }) { Text(stringResource(R.string.cancel)) } },
    )
    if (licenses) LicenseDialog { licenses = false }
}

@Composable
private fun LicenseDialog(onClose: () -> Unit) {
    val context = LocalContext.current
    val appVersion = remember {
        runCatching {
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        }.getOrNull().orEmpty()
    }
    val notices by produceState("") {
        value = withContext(Dispatchers.IO) {
            context.assets.list("licenses").orEmpty().sorted().joinToString("\n\n") { name ->
                name + "\n" + context.assets.open("licenses/$name").bufferedReader().use { it.readText() }
            }
        }
    }
    AlertDialog(onDismissRequest = onClose,
        title = { Text(stringResource(R.string.licenses) + if (appVersion.isNotEmpty()) " · RawLab ZSPro $appVersion" else "") },
        text = { Text(notices, Modifier.heightIn(max = 440.dp).verticalScroll(rememberScrollState()), style = MaterialTheme.typography.bodySmall) },
        confirmButton = { TextButton(onClick = onClose) { Text(stringResource(R.string.confirm)) } })
}
