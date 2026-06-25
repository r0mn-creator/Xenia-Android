package jp.xenia.emulator;

import android.app.Dialog;
import android.content.Intent;
import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

public class GamePropertiesDialog extends DialogFragment {

    private static final String ARG_INDEX = "index";
    public static final String TAG = "GamePropertiesDialog";

    // Caller must set this before showing the dialog to handle image picker result
    interface Listener {
        void onSetCustomArt(int gameIndex);
    }

    public static GamePropertiesDialog newInstance(int index) {
        final GamePropertiesDialog d = new GamePropertiesDialog();
        final Bundle args = new Bundle();
        args.putInt(ARG_INDEX, index);
        d.setArguments(args);
        return d;
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        final int index = requireArguments().getInt(ARG_INDEX);
        final LauncherActivity.GameEntry game = LauncherActivity.sGames.get(index);

        final String[] options = {
                "Game Details",
                "Launch Game",
                "Set Custom Box Art",
                "Clear Box Art",
                "Game Settings",
                "Remove from Library"
        };

        return new MaterialAlertDialogBuilder(requireContext())
                .setTitle(game.title)
                .setItems(options, (dialog, which) -> handleOption(which, index, game))
                .create();
    }

    private void handleOption(int which, int index, LauncherActivity.GameEntry game) {
        switch (which) {
            case 0: // Game Details
                GameDetailsDialog.newInstance(index)
                        .show(requireActivity().getSupportFragmentManager(), "game_details");
                break;

            case 1: // Launch
                Toast.makeText(requireContext(), "Launching: " + game.title, Toast.LENGTH_SHORT).show();
                break;

            case 2: // Set Custom Box Art
                if (getActivity() instanceof Listener) {
                    ((Listener) getActivity()).onSetCustomArt(index);
                }
                break;

            case 3: // Clear Box Art
                game.customArtUri = null;
                BoxArtManager.clearCache(requireContext(), game);
                notifyGrids();
                Toast.makeText(requireContext(), "Box art cleared", Toast.LENGTH_SHORT).show();
                break;

            case 4: // Game Settings
                startActivity(new Intent(requireContext(), SettingsActivity.class));
                break;

            case 5: // Remove
                new MaterialAlertDialogBuilder(requireContext())
                        .setTitle("Remove Game")
                        .setMessage("Remove \"" + game.title + "\" from your library?")
                        .setPositiveButton("Remove", (d, w) -> {
                            LauncherActivity.sGames.remove(index);
                            notifyGrids();
                        })
                        .setNegativeButton("Cancel", null)
                        .show();
                break;
        }
    }

    private void notifyGrids() {
        if (getActivity() == null) return;
        ((androidx.appcompat.app.AppCompatActivity) getActivity())
                .getSupportFragmentManager().getFragments().forEach(f -> {
                    if (f instanceof GameGridFragment) ((GameGridFragment) f).refresh();
                });
    }
}
